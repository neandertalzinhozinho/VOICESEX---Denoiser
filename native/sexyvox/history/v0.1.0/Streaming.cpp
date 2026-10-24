#include "Streaming.h"
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
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&environment),&module)) throw std::runtime_error("Module path unavailable");
        wchar_t file[32768]{}; GetModuleFileNameW(module,file,32768);
        std::wstring path(file); path=path.substr(0,path.find_last_of(L"\\/"))+L"\\onnxruntime.dll";
        if(!LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS))
            throw std::runtime_error("Missing ONNX Runtime next to plugin");
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
            for(int c=0;c<channels;++c) {
                auto* dst=context.data.data()+c*history*bins;
                std::move(dst+bins,dst+history*bins,dst);
                std::copy_n(src.data.data()+c*bins,bins,dst+(history-1)*bins);
            }
        }
        return out;
    }
};
constexpr double pi=3.14159265358979323846;
}

struct StreamChannel::Impl {
    Graph dfEnc{BinaryData::df_enc_onnx,BinaryData::df_enc_onnxSize,true,256,2,2,108,"feat_spec"};
    Graph dfDec{BinaryData::df_dec_onnx,BinaryData::df_dec_onnxSize,false,256,64,5,108,"c0"};
    Graph erEnc{BinaryData::erb_enc_onnx,BinaryData::erb_enc_onnxSize,true,96,1,2,56,"feat_erb"};
    Graph erDec{BinaryData::erb_dec_onnx,BinaryData::erb_dec_onnxSize,false,96};
    juce::dsp::FFT dfFFT{10}, erFFT{8};
    std::array<float,1024> input{},win{},tail{};
    std::array<float,256> erInput{},erWin{};
    std::array<double,108> norm{};
    std::array<std::array<std::complex<float>,513>,6> spectra{};
    std::array<float,56> mask{};
    std::array<int,56> widths{};
    std::array<int,513> bands{};
    Impl() {
        for(int i=0;i<1024;++i) win[i]=float(std::sin(pi/2*std::pow(std::sin(pi*(i+.5)/1024),2)));
        for(int i=0;i<256;++i) erWin[i]=float(std::sin(pi/2*std::pow(std::sin(pi*(i+.5)/256),2)));
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
        // ERB frames 4t..4t+3, with the mask at 4t used by DF frame t.
        for(int step=0;step<4;++step) {
            std::copy(erInput.begin()+128,erInput.end(),erInput.begin());
            std::copy_n(x+step*128,128,erInput.begin()+128);
            std::array<float,512> buf{};
            for(int i=0;i<256;++i) buf[i]=erInput[i]*erWin[i];
            erFFT.performRealOnlyForwardTransform(buf.data(),true);
            Tensor feature({1,1,1,56}); int edge=0;
            for(int b=0;b<56;++b) {
                double power=0;
                for(int f=edge;f<edge+widths[b];++f) power+=(double(buf[2*f])*buf[2*f]+double(buf[2*f+1])*buf[2*f+1])/(256.0*256.0);
                feature.data[b]=float(10*std::log10(power/widths[b]+1e-10)); edge+=widths[b];
            }
            auto out=erDec.run(erEnc.run({{"feat_erb",std::move(feature)}}));
            if(step==0) std::copy_n(out.at("m").data.begin(),56,mask.begin());
        }
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
        for(int f=0;f<513;++f) {
            std::complex<float> v{};
            if(f<108) {
                for(int k=0;k<8;++k) {
                    std::complex<float> s=k<6?spectra[k][f]:(k==6?spectra[5][f+1]:(f?spectra[5][f-1]:std::complex<float>{}));
                    v+=s*std::complex<float>(co[(f*8+k)*2],co[(f*8+k)*2+1]);
                }
            } else v=spectra[5][f]*mask[bands[f]];
            buf[2*f]=v.real()*1024; buf[2*f+1]=v.imag()*1024;
        }
        dfFFT.performRealOnlyInverseTransform(buf.data());
        for(int i=0;i<512;++i) {
            voice[i]=tail[i]+buf[i]*win[i];
            tail[i]=buf[i+512]*win[i+512];
        }
    }
};
StreamChannel::StreamChannel():p(std::make_unique<Impl>()){}
StreamChannel::~StreamChannel()=default;
void StreamChannel::process(const float* input,float* voice){p->process(input,voice);}
