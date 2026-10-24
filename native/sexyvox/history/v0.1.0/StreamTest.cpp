#include "Streaming.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3) return 2;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[1])));
    if(!reader || reader->sampleRate!=48000) return 3;
    juce::AudioBuffer<float> x(int(reader->numChannels),int(reader->lengthInSamples));
    reader->read(&x,0,x.getNumSamples(),0,true,true);
    juce::AudioBuffer<float> y(x.getNumChannels(),x.getNumSamples());y.clear();
    double elapsed=0;
    for(int c=0;c<x.getNumChannels();++c) {
        StreamChannel stream;
        auto start=std::chrono::steady_clock::now();
        for(int pos=0;pos<x.getNumSamples()+512;pos+=512) {
            std::array<float,512> input{},voice{};
            for(int i=0;i<512 && pos+i<x.getNumSamples();++i) input[i]=x.getSample(c,pos+i);
            stream.process(input.data(),voice.data());
            if(pos>=512) for(int i=0;i<512 && pos-512+i<x.getNumSamples();++i) y.setSample(c,pos-512+i,voice[i]);
        }
        elapsed+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    }
    juce::File file(argv[2]);if(file.exists()) return 4;
    auto output=file.createOutputStream();juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(output.release(),48000,(unsigned)y.getNumChannels(),24,{},0));
    if(!writer || !writer->writeFromAudioSampleBuffer(y,0,y.getNumSamples())) return 5;
    std::cout<<"RTF="<<elapsed/(x.getNumSamples()/48000.0)<<" seconds="<<elapsed<<std::endl;
}
