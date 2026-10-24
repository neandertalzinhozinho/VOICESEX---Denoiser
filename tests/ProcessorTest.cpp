#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <fstream>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int sexyUnderruns(juce::AudioProcessor*);

void parameter(juce::AudioProcessor& p,const juce::String& name,float value) {
    for(auto* a:p.getParameters()) if(a->getName(64).equalsIgnoreCase(name)) {
        auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(a);
        a->setValueNotifyingHost(ranged->convertTo0to1(value));return;
    }
    throw std::runtime_error("Missing parameter");
}

std::vector<float> render(juce::AudioProcessor& p,const std::vector<float>& x,int block,bool realtime,double rate=48000.0) {
    p.setNonRealtime(!realtime);p.prepareToPlay(rate,block);
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
        if(realtime) std::this_thread::sleep_until(start+std::chrono::microseconds(int64_t(pos)*1000000/int64_t(rate)));
    }
    return y;
}

int soak(juce::AudioProcessor& p,const char* source,const char* report) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(source)));
    if(!reader || reader->sampleRate!=48000 || reader->lengthInSamples<1024) throw std::runtime_error("Invalid soak source");
    juce::AudioBuffer<float> speech(1,int(reader->lengthInSamples));
    reader->read(&speech,0,speech.getNumSamples(),0,true,false);
    constexpr int count=48000*30;
    juce::AudioBuffer<float> input(2,count),reference(2,count),actual(2,count);
    for(int i=0;i<count;++i) {
        input.setSample(0,i,speech.getSample(0,i%speech.getNumSamples()));
        input.setSample(1,i,.71f*speech.getSample(0,(i+137)%speech.getNumSamples())+.008f*std::sin(float(i)*.113f));
    }
    parameter(p,"voice",0);parameter(p,"noise",-30);parameter(p,"bypass",0);
    auto run=[&](int block,bool realtime,juce::AudioBuffer<float>& output) {
        p.setNonRealtime(!realtime);p.prepareToPlay(48000,block);
        juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer midi;
        auto start=std::chrono::steady_clock::now();double maxCallback=0;
        for(int pos=0;pos<count;) {
            int n=std::min(block,count-pos);buffer.setSize(2,n,false,false,true);
            for(int c=0;c<2;++c) buffer.copyFrom(c,0,input,c,pos,n);
            auto before=std::chrono::steady_clock::now();p.processBlock(buffer,midi);
            maxCallback=std::max(maxCallback,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count());
            for(int c=0;c<2;++c) output.copyFrom(c,pos,buffer,c,0,n);
            pos+=n;
            if(realtime) std::this_thread::sleep_until(start+std::chrono::microseconds(int64_t(pos)*1000000/48000));
        }
        std::cout<<"run block="<<block<<" realtime="<<realtime<<" max_callback_ms="<<maxCallback<<" underruns="<<sexyUnderruns(&p)<<std::endl;
        return maxCallback;
    };
    run(512,false,reference);
    std::ofstream file(report,std::ios::out);if(!file) throw std::runtime_error("Report open failed");
    file<<"{\"seconds_per_case\":30,\"channels\":2,\"sample_rate\":48000,\"latency_samples\":"<<p.getLatencySamples()<<",\"cases\":[";
    bool first=true;bool passed=true;
    for(int block:{64,1024}) {
        double callback=run(block,true,actual);float error=0;
        for(int c=0;c<2;++c) for(int i=0;i<count;++i) {
            float y=actual.getSample(c,i);
            if(!std::isfinite(y)) throw std::runtime_error("Nonfinite soak output");
            error=std::max(error,std::abs(y-reference.getSample(c,i)));
        }
        int misses=sexyUnderruns(&p);passed=passed && misses==0 && error<1e-5f;
        if(!first) file<<",";first=false;
        file<<"{\"block\":"<<block<<",\"underruns\":"<<misses<<",\"max_error\":"<<error<<",\"max_callback_ms\":"<<callback<<"}";
        file.flush();std::cout<<"max_error="<<error<<std::endl;
    }
    file<<"],\"passed\":"<<(passed?"true":"false")<<"}";
    p.releaseResources();return passed?0:1;
}

int renderFile(juce::AudioProcessor& p, double rate, int block, float noise, float voice, float bypass, const char* inPath, const char* outPath) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(inPath)));
    if (!reader) { std::cerr << "Cannot open " << inPath << std::endl; return 1; }
    int numSamples = int(reader->lengthInSamples);
    int channels = std::clamp(int(reader->numChannels), 1, 2);
    juce::AudioBuffer<float> inBuf(2, numSamples);
    reader->read(&inBuf, 0, numSamples, 0, true, true);
    if (channels == 1) inBuf.copyFrom(1, 0, inBuf, 0, 0, numSamples);

    parameter(p, "noise", noise);
    parameter(p, "voice", voice);
    parameter(p, "bypass", bypass);

    p.setNonRealtime(true);
    p.prepareToPlay(rate, block);

    juce::AudioBuffer<float> outBuf(2, numSamples);
    juce::AudioBuffer<float> blockBuf(2, block);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < numSamples; ) {
        int n = std::min(block, numSamples - pos);
        blockBuf.setSize(2, n, false, false, true);
        for (int c = 0; c < 2; ++c) blockBuf.copyFrom(c, 0, inBuf, c, pos, n);
        p.processBlock(blockBuf, midi);
        for (int c = 0; c < 2; ++c) outBuf.copyFrom(c, pos, blockBuf, c, 0, n);
        pos += n;
    }
    p.releaseResources();

    juce::File outFile(outPath);
    if (outFile.exists()) outFile.deleteFile();
    auto os = outFile.createOutputStream();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(os.release(), rate, 2, 24, {}, 0));
    if (!writer) { std::cerr << "Cannot create output " << outPath << std::endl; return 2; }
    writer->writeFromAudioSampleBuffer(outBuf, 0, numSamples);
    std::cout << "Rendered " << numSamples << " samples at " << rate << " Hz to " << outPath << " (latency=" << p.getLatencySamples() << ")" << std::endl;
    return 0;
}

int main(int argc,char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
        if(argc==4 && juce::String(argv[1])=="--soak") return soak(*p,argv[2],argv[3]);
        if(argc==9 && juce::String(argv[1])=="--render") return renderFile(*p, std::stod(argv[2]), std::stoi(argv[3]), std::stof(argv[4]), std::stof(argv[5]), std::stof(argv[6]), argv[7], argv[8]);
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
        constexpr int delay=2048;
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

        // Robustness testing for host state restoration (FL Studio / VST3 variations)
        try {
            // Test alternative root tag names
            juce::String stateStr(static_cast<const char*>(saved.getData()), saved.getSize());
            auto xml = juce::XmlDocument::parse(stateStr);
            if (xml != nullptr) {
                xml->setTagName("PARAMETERS");
                auto s1 = xml->toString();
                p->setStateInformation(s1.toRawUTF8(), (int)s1.getNumBytesAsUTF8());

                xml->setTagName("AudioProcessorValueTreeState");
                auto s2 = xml->toString();
                p->setStateInformation(s2.toRawUTF8(), (int)s2.getNumBytesAsUTF8());
            }
            // Test empty / null / malformed states
            p->setStateInformation(nullptr, 0);
            p->setStateInformation(saved.getData(), 0);
            const char* garbage = "<garbage>invalid</garbage>";
            p->setStateInformation(garbage, (int)std::strlen(garbage));
        } catch(...) {
            throw std::runtime_error("State robustness test exception");
        }
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
        std::cout<<"PASS 48 kHz: unity, reported latency, stereo, separation, blocks 64/128/512/777, state recall, bypass, realtime"<<std::endl;

        // =====================================================================
        // 44.1 kHz Validation Suite
        // =====================================================================
        p->prepareToPlay(44100.0, 128);
        int delay44 = p->getLatencySamples();
        std::cout << "44.1 kHz reported latency: " << delay44 << " samples" << std::endl;
        if (delay44 != 1943) throw std::runtime_error("44.1 kHz latency mismatch (expected 1943)");

        parameter(*p, "voice", 0);
        parameter(*p, "noise", 0);
        parameter(*p, "bypass", 0);

        // 4 seconds of 1 kHz sine at 44.1 kHz
        std::vector<float> input44(44100 * 4);
        for (size_t i = 0; i < input44.size(); ++i) {
            input44[i] = 0.5f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 1000.0 * double(i) / 44100.0));
        }

        auto unity44 = render(*p, input44, 128, false, 44100.0);
        float unityErr44 = 0;
        for (size_t i = delay44 + 64; i < input44.size(); ++i) {
            unityErr44 = std::max(unityErr44, std::abs(unity44[i] - input44[i - delay44]));
        }
        std::cout << "44.1 kHz unity max error vs delayed sine: " << unityErr44 << std::endl;
        if (unityErr44 > 0.005f) throw std::runtime_error("44.1 kHz unity accuracy failure");

        // Separation test at 44.1 kHz
        parameter(*p, "noise", -50);
        auto separated44 = render(*p, input44, 128, false, 44100.0);
        auto other64_44  = render(*p, input44, 64,  false, 44100.0);
        auto other512_44 = render(*p, input44, 512, false, 44100.0);
        auto other777_44 = render(*p, input44, 777, false, 44100.0);

        float change44 = 0;
        for (size_t i = delay44; i < input44.size(); ++i) {
            change44 += std::abs(separated44[i] - unity44[i]);
        }
        if (change44 < 1.0f) throw std::runtime_error("44.1 kHz separation failure");

        // Block invariance at 44.1 kHz
        float blockErr64 = 0, blockErr512 = 0, blockErr777 = 0;
        for (size_t i = delay44; i < input44.size(); ++i) {
            blockErr64  = std::max(blockErr64,  std::abs(separated44[i] - other64_44[i]));
            blockErr512 = std::max(blockErr512, std::abs(separated44[i] - other512_44[i]));
            blockErr777 = std::max(blockErr777, std::abs(separated44[i] - other777_44[i]));
        }
        std::cout << "44.1 kHz block diffs vs 128: 64=" << blockErr64 << " 512=" << blockErr512 << " 777=" << blockErr777 << std::endl;
        if (blockErr64 > 1e-4f || blockErr512 > 1e-4f || blockErr777 > 1e-4f) {
            throw std::runtime_error("44.1 kHz block invariance failure");
        }

        // Realtime deadline test at 44.1 kHz
        auto live44 = render(*p, input44, 128, true, 44100.0);
        auto misses44 = sexyUnderruns(p.get());
        float liveErr44 = 0;
        for (size_t i = delay44; i < input44.size(); ++i) {
            liveErr44 = std::max(liveErr44, std::abs(live44[i] - separated44[i]));
        }
        std::cout << "44.1 kHz realtime_stereo_128 underruns=" << misses44 << " max_vs_offline=" << liveErr44 << std::endl;
        if (misses44 || liveErr44 > 1e-4f) throw std::runtime_error("44.1 kHz realtime deadline failure");

        p->releaseResources();
        std::cout << "PASS 44.1 kHz: latency=1943, unity sine, separation, block invariance {64,128,512,777}, realtime" << std::endl;
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
