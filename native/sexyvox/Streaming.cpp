#define _USE_MATH_DEFINES
#include "Streaming.h"
#include "Combinador.h"
#include "BinaryData.h"
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <vector>
#include <string>
#include <complex>
#include <cmath>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float> data;
    Tensor() = default;
    Tensor(std::initializer_list<int64_t> dims) : shape(dims) {
        size_t n=1; for(auto d:shape) n*=size_t(d); data.resize(n);
    }
};
using Tensors = std::map<std::string, Tensor>;
Ort::Env& environment() {
    static const bool loaded=[] {
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&environment),&module);
        wchar_t file[32768]{};
        if(module) {
            GetModuleFileNameW(module,file,32768);
        } else {
            GetModuleFileNameW(nullptr,file,32768);
        }
        std::wstring path(file);
        auto pos=path.find_last_of(L"\\/");
        if(pos!=std::wstring::npos) {
            path=path.substr(0,pos)+L"\\onnxruntime.dll";
        } else {
            path=L"onnxruntime.dll";
        }
        HMODULE hOr=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!hOr) {
            hOr=LoadLibraryW(L"onnxruntime.dll");
        }
        if(!hOr) throw std::runtime_error("Missing ONNX Runtime DLL next to module or application");
        Ort::InitApi();
        return true;
    }();
    (void)loaded;
    static Ort::Env env(ORT_LOGGING_LEVEL_ERROR,"SEXYVOX"); return env;
}
struct Graph {
    std::unique_ptr<Ort::Session> session;
    Tensor hidden, context;
    std::string source;
    int channels=0, history=0, bins=0;
    std::vector<std::string> inputs, outputs;
    Graph(const void* bytes, size_t count, bool encoder, int width, int c=0, int h=0, int b=0, std::string src="")
        : hidden({encoder?1:2,1,width}), source(std::move(src)), channels(c),history(h),bins(b) {
        (void)environment();
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1); opts.SetInterOpNumThreads(1);
        opts.AddConfigEntry("session.intra_op.allow_spinning","0");
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session=std::make_unique<Ort::Session>(environment(),bytes,count,opts);
        Ort::AllocatorWithDefaultOptions allocator;
        for(size_t i=0;i<session->GetInputCount();++i) inputs.emplace_back(session->GetInputNameAllocated(i,allocator).get());
        for(size_t i=0;i<session->GetOutputCount();++i) outputs.emplace_back(session->GetOutputNameAllocated(i,allocator).get());
        if(c) context=Tensor({1,c,h,b});
    }
    Tensors run(Tensors feed) {
        feed["h"]=hidden;
        if(channels) feed["conv_history"]=context;
        auto mem=Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
        std::vector<const char*> inNames,outNames;
        std::vector<Ort::Value> values;
        for(auto& name:inputs) {
            auto& t=feed.at(name); inNames.push_back(name.c_str());
            values.emplace_back(Ort::Value::CreateTensor<float>(mem,t.data.data(),t.data.size(),t.shape.data(),t.shape.size()));
        }
        for(auto& name:outputs) outNames.push_back(name.c_str());
        auto result=session->Run(Ort::RunOptions{nullptr},inNames.data(),values.data(),values.size(),outNames.data(),outNames.size());
        Tensors out;
        for(size_t i=0;i<outputs.size();++i) {
            auto info=result[i].GetTensorTypeAndShapeInfo();
            Tensor t; t.shape=info.GetShape();
            auto* ptr=result[i].GetTensorData<float>();
            t.data.assign(ptr,ptr+info.GetElementCount());
            for(float v:t.data) if(!std::isfinite(v)) throw std::runtime_error("Nonfinite inference");
            out.emplace(outputs[i],std::move(t));
        }
        hidden=out.at("history");
        if(channels) {
            auto& src=feed.at(source);
            const int frames=int(src.shape[2]);
            for(int c=0;c<channels;++c) {
                auto* dst=context.data.data()+c*history*bins;
                const auto* from=src.data.data()+c*frames*bins;
                if(frames>=history) {
                    std::copy_n(from+(frames-history)*bins,history*bins,dst);
                } else {
                    std::move(dst+frames*bins,dst+history*bins,dst);
                    std::copy_n(from,frames*bins,dst+(history-frames)*bins);
                }
            }
        }
        return out;
    }
};
}

struct StreamChannel::Impl {
    Graph dfEnc{BinaryData::df_enc_onnx, BinaryData::df_enc_onnxSize, true, 256, 2, 2, 108, "feat_spec"};
    Graph dfDec{BinaryData::df_dec_onnx, BinaryData::df_dec_onnxSize, false, 256, 64, 5, 108, "c0"};
    Graph erEnc{BinaryData::erb_enc_onnx, BinaryData::erb_enc_onnxSize, true, 96, 1, 2, 56, "feat_erb"};
    Graph erDec{BinaryData::erb_dec_onnx, BinaryData::erb_dec_onnxSize, false, 96};
    juce::dsp::FFT dfFFT{10};
    juce::dsp::FFT erFFT{8};
    std::array<float,1024> input{};
    std::array<float,256> erInput{};
    std::array<float,1024> win{};
    std::array<float,256> erWin{};
    std::array<double,108> norm{};
    std::array<std::array<std::complex<float>,513>,6> spectra{};
    std::array<float,56> mask{};
    std::array<int,56> widths{};
    std::array<int,513> bands{};
    std::array<float,512> tail_d{};
    std::array<float,512> tail_e{};
    Combinador combinador;

    Impl() {
        for(int i=0;i<1024;++i) win[i]=float(std::sin(M_PI/2*std::pow(std::sin(M_PI*(i+.5)/1024),2)));
        for(int i=0;i<256;++i) erWin[i]=float(std::sin(M_PI/2*std::pow(std::sin(M_PI*(i+.5)/256),2)));
        for(int i=0;i<108;++i) norm[i]=.001+(.0001-.001)*i/107;
        double high=9.265*std::log1p(24000/(24.7*9.265)); int prev=0,over=0,sum=0;
        for(int i=1;i<=56;++i) {
            double freq=std::expm1(i*high/56/9.265)*24.7*9.265;
            int b=int(std::floor(freq/187.5+.5)),count=b-prev-over;
            over=std::max(0,1-count); widths[i-1]=std::max(1,count); prev=b; sum+=widths[i-1];
        }
        widths[55]+=129-sum;
        for(int f=0;f<513;++f) {
            int edge=0,b=0;
            for(;b<55;++b) { edge+=widths[b]; if(f/4.0<edge) break; }
            bands[f]=b;
        }
    }
    void process(const float* x,float* voice) {
        Tensor erbFeature({1,1,4,56});
        for(int step=0;step<4;++step) {
            std::copy(erInput.begin()+128,erInput.end(),erInput.begin());
            std::copy_n(x+step*128,128,erInput.begin()+128);
            std::array<float,512> buf{};
            for(int i=0;i<256;++i) buf[i]=erInput[i]*erWin[i];
            erFFT.performRealOnlyForwardTransform(buf.data(),true);
            int edge=0;
            for(int b=0;b<56;++b) {
                double power=0;
                for(int f=edge;f<edge+widths[b];++f) power+=(double(buf[2*f])*buf[2*f]+double(buf[2*f+1])*buf[2*f+1])/(256.0*256.0);
                erbFeature.data[step*56+b]=float(10*std::log10(power/widths[b]+1e-10)); edge+=widths[b];
            }
        }
        auto erbOut=erDec.run(erEnc.run({{"feat_erb",std::move(erbFeature)}}));
        std::copy_n(erbOut.at("m").data.begin(),56,mask.begin());
        std::copy(input.begin()+512,input.end(),input.begin());
        std::copy_n(x,512,input.begin()+512);
        std::array<float,2048> buf{};
        for(int i=0;i<1024;++i) buf[i]=input[i]*win[i];
        dfFFT.performRealOnlyForwardTransform(buf.data(),true);
        for(int i=0;i<5;++i) spectra[i]=spectra[i+1];
        for(int f=0;f<513;++f) spectra[5][f]={buf[2*f]/1024,buf[2*f+1]/1024};
        Tensor feature({1,2,1,108}); const double alpha=std::exp(-512.0/48000);
        for(int f=0;f<108;++f) {
            norm[f]=alpha*norm[f]+(1-alpha)*std::abs(spectra[5][f]);
            auto v=spectra[5][f]/float(std::sqrt(std::max(norm[f],1e-20)));
            feature.data[f]=v.real(); feature.data[108+f]=v.imag();
        }
        auto out=dfDec.run(dfEnc.run({{"feat_spec",std::move(feature)}}));
        auto& co=out.at("coefs").data;

        std::array<float, 513> mask_ramp{};
        int edge = 0;
        for (int b = 0; b < 56; ++b) {
            int lo = edge * 4;
            int hi = std::min(513, (edge + widths[b]) * 4);
            int w = hi - lo;
            float m0 = mask[b];
            float m1 = (b + 1 < 56) ? mask[b + 1] : m0;
            float step = (w > 0) ? (m1 - m0) / float(w) : 0.0f;
            for (int i = 0; i < w && (lo + i) < 513; ++i) {
                mask_ramp[lo + i] = m0 + float(i) * step;
            }
            edge += widths[b];
        }

        std::array<float,2048> out_buf{};
        for(int f=0;f<513;++f) {
            std::complex<float> v{};
            if(f<108) {
                std::complex<float> v_df{};
                for(int k=0;k<8;++k) {
                    std::complex<float> s=k<6?spectra[k][f]:(k==6?spectra[5][f+1]:(f?spectra[5][f-1]:std::complex<float>{}));
                    v_df+=s*std::complex<float>(co[(f*8+k)*2],co[(f*8+k)*2+1]);
                }
                float freq = f * (48000.0f / 1024.0f);
                float blend = std::clamp((freq - 2000.0f) / (5200.0f - 2000.0f), 0.0f, 1.0f);
                float m = mask_ramp[f];
                v = v_df * (1.0f - blend * (1.0f - m));
            } else {
                v = spectra[5][f] * mask_ramp[f];
            }
            out_buf[2*f]=v.real()*1024; out_buf[2*f+1]=v.imag()*1024;
        }

        dfFFT.performRealOnlyInverseTransform(out_buf.data());
        for(int i=0;i<512;++i) {
            voice[i]=tail_d[i]+out_buf[i]*win[i];
            tail_d[i]=out_buf[i+512]*win[i+512];
        }
    }
};
StreamChannel::StreamChannel(){
    (void)environment();
    p = std::make_unique<Impl>();
}
StreamChannel::~StreamChannel()=default;
void StreamChannel::reset(){
    p->input.fill(0);p->erInput.fill(0);
    for(int i=0;i<108;++i) p->norm[i]=.001+(.0001-.001)*i/107;
    for(auto& s:p->spectra) s.fill(0);
    p->mask.fill(0);p->tail_d.fill(0);p->tail_e.fill(0);
    p->dfEnc.hidden.data.assign(p->dfEnc.hidden.data.size(),0);
    if(p->dfEnc.channels) p->dfEnc.context.data.assign(p->dfEnc.context.data.size(),0);
    p->dfDec.hidden.data.assign(p->dfDec.hidden.data.size(),0);
    if(p->dfDec.channels) p->dfDec.context.data.assign(p->dfDec.context.data.size(),0);
    p->erEnc.hidden.data.assign(p->erEnc.hidden.data.size(),0);
    if(p->erEnc.channels) p->erEnc.context.data.assign(p->erEnc.context.data.size(),0);
    p->erDec.hidden.data.assign(p->erDec.hidden.data.size(),0);
    p->combinador.reset();
}
void StreamChannel::process(const float* x,float* voice){p->process(x,voice);}
