#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int sexyUnderruns(juce::AudioProcessor*);

void parameter(juce::AudioProcessor& p,const juce::String& name,float value) {
    for(auto* a:p.getParameters()) if(a->getName(64).equalsIgnoreCase(name)) {
        auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(a);
        a->setValueNotifyingHost(ranged->convertTo0to1(value));return;
    }
    throw std::runtime_error("Missing parameter");
}

std::vector<float> render(juce::AudioProcessor& p,const std::vector<float>& x,int block,bool realtime) {
    p.setNonRealtime(!realtime);p.prepareToPlay(48000,block);
    std::vector<float> y(x.size());juce::MidiBuffer midi;
    auto start=std::chrono::steady_clock::now();
    for(size_t pos=0;pos<x.size();) {
        int n=int(std::min(size_t(block),x.size()-pos));
        juce::AudioBuffer<float> b(2,n);
        for(int c=0;c<2;++c) std::copy_n(x.data()+pos,n,b.getWritePointer(c));
        p.processBlock(b,midi);
        for(int i=0;i<n;++i) {
            if(!std::isfinite(b.getSample(0,i)) || std::abs(b.getSample(0,i)-b.getSample(1,i))>1e-5f)
                throw std::runtime_error("Stereo/nonfinite failure");
            y[pos+i]=b.getSample(0,i);
        }
        pos+=n;
        if(realtime) std::this_thread::sleep_until(start+std::chrono::microseconds(int64_t(pos)*1000000/48000));
    }
    return y;
}

int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
        if(argc==2) {
            std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
            editor->setVisible(true);
            juce::Image image(juce::Image::ARGB,editor->getWidth()*2,editor->getHeight()*2,true);
            {
                juce::Graphics g(image);g.addTransform(juce::AffineTransform::scale(2.f));
                editor->paintEntireComponent(g,true);
            }
            juce::File file(argv[1]);if(file.exists()) return 2;
            auto output=file.createOutputStream();juce::PNGImageFormat png;
            return output && png.writeImageToStream(image,*output)?0:3;
        }
        constexpr int delay=8192;
        std::vector<float> input(48000*4);
        for(size_t i=0;i<input.size();++i) input[i]=.1f*std::sin(float(i)*.037f)+.02f*std::sin(float(i)*.151f);
        auto unity=render(*p,input,128,false);
        float err=0;for(size_t i=delay;i<input.size();++i) err=std::max(err,std::abs(unity[i]-input[i-delay]));
        if(err>1e-6 || p->getLatencySamples()!=delay) throw std::runtime_error("Unity/latency failure");
        parameter(*p,"noise",-50);
        auto separated=render(*p,input,128,false);
        auto other=render(*p,input,777,false);
        err=0;float change=0;
        for(size_t i=delay;i<input.size();++i) {err=std::max(err,std::abs(separated[i]-other[i]));change+=std::abs(separated[i]-unity[i]);}
        if(err>1e-5 || change<1) throw std::runtime_error("Block invariance/separation failure");
        juce::MemoryBlock saved;p->getStateInformation(saved);
        parameter(*p,"noise",0);p->setStateInformation(saved.getData(),int(saved.getSize()));
        auto restored=render(*p,input,512,false);
        err=0;for(size_t i=0;i<input.size();++i) err=std::max(err,std::abs(restored[i]-separated[i]));
        if(err>1e-5) throw std::runtime_error("State recall failure");
        parameter(*p,"bypass",1);auto bypass=render(*p,input,64,false);
        err=0;for(size_t i=delay;i<input.size();++i) err=std::max(err,std::abs(bypass[i]-input[i-delay]));
        if(err>1e-6) throw std::runtime_error("Bypass failure");
        parameter(*p,"bypass",0);
        auto live=render(*p,input,128,true);
        auto misses=sexyUnderruns(p.get());
        err=0;for(size_t i=delay;i<input.size();++i) err=std::max(err,std::abs(live[i]-separated[i]));
        std::cout<<"realtime_stereo_128 underruns="<<misses<<" max_vs_offline="<<err<<std::endl;
        if(misses || err>1e-5) throw std::runtime_error("Realtime deadline failure");
        p->releaseResources();
        std::cout<<"PASS unity, reported latency, stereo, separation, blocks 64/128/512/777, state recall, bypass, realtime"<<std::endl;
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
