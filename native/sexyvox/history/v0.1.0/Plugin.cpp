#include <juce_audio_utils/juce_audio_utils.h>
#include "Streaming.h"
#include <atomic>
#include <thread>
#include <chrono>

namespace {
constexpr int hop=512, latency=8192;
struct Packet { int64_t index=0; std::array<std::array<float,hop>,2> data{}; };
template<int N> struct Queue {
    std::array<Packet,N> packets;
    std::atomic<unsigned> read{0},write{0};
    bool push(const Packet& p) {
        auto w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)>=N) return false;
        packets[w%N]=p; write.store(w+1,std::memory_order_release); return true;
    }
    bool pop(Packet& p) {
        auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire)) return false;
        p=packets[r%N]; read.store(r+1,std::memory_order_release); return true;
    }
    void reset(){read=0;write=0;}
};
}

class SexyProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState state;
    std::atomic<bool> ready{false},failed{false},supported{true};
    std::atomic<int> underruns{0};
    SexyProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"SEXYVOX",layout()) {}
    ~SexyProcessor() override { releaseResources(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        for(auto name:{"voice","noise"}) p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{name,1},name,
            juce::NormalisableRange<float>(-50,20,.1f),0,juce::AudioParameterFloatAttributes().withLabel("dB")));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false)); return p;
    }
    const juce::String getName() const override{return "SEXYVOX";}
    bool acceptsMidi() const override{return false;} bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return double(latency+1024)/48000;}
    int getNumPrograms() override{return 1;} int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{} const juce::String getProgramName(int) override{return "Default";}
    void changeProgramName(int,const juce::String&) override{}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return (l.getMainInputChannelSet()==juce::AudioChannelSet::mono() || l.getMainInputChannelSet()==juce::AudioChannelSet::stereo())
            && l.getMainInputChannelSet()==l.getMainOutputChannelSet();
    }
    juce::AudioProcessorParameter* getBypassParameter() const override {return state.getParameter("bypass");}
    void prepareToPlay(double rate,int) override {
        releaseResources();
        supported=std::abs(rate-48000)<.01; failed=false; ready=false; underruns=0;
        in.reset();out.reset(); counter=0; pendingValid=false; voiceValid=false;
        incoming=Packet{}; current=Packet{}; for(auto& c:dry) c.fill(0);
        gv.reset(rate,.02);gn.reset(rate,.02);wet.reset(rate,.02);
        gv.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("voice")->load()));
        gn.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("noise")->load()));
        wet.setCurrentAndTargetValue(state.getRawParameterValue("bypass")->load()>.5f?0.f:1.f);
        setLatencySamples(supported?latency:0);
        if(!supported) return;
        const int channels=getTotalNumInputChannels();
        std::array<std::unique_ptr<StreamChannel>,2> streams;
        try {
            for(int c=0;c<channels;++c) streams[c]=std::make_unique<StreamChannel>();
        } catch(...) {failed=true;return;}
        running=true;ready=true;
        worker=std::thread([this,channels,streams=std::move(streams)]() mutable {
            try {
                Packet input,output; int64_t expected=0;
                while(running.load(std::memory_order_acquire)) {
                    if(!in.pop(input)) {std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
                    if(input.index!=expected) {failed=true;break;}
                    ++expected; output.index=input.index-1;
                    for(int c=0;c<channels;++c) streams[c]->process(input.data[c].data(),output.data[c].data());
                    if(output.index>=0) {
                        while(running && !out.push(output)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            } catch(...) {failed=true; ready=false;}
        });
    }
    void releaseResources() override {
        running=false; if(worker.joinable()) worker.join(); ready=false;
    }
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override {process(b,false);}
    void processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override {process(b,true);}
    void process(juce::AudioBuffer<float>& b,bool hostBypass) {
        juce::ScopedNoDenormals noDenormals;
        if(!supported) return;
        gv.setTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("voice")->load()));
        gn.setTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("noise")->load()));
        wet.setTargetValue(hostBypass || state.getRawParameterValue("bypass")->load()>.5f?0.f:1.f);
        int channels=juce::jmin(2,b.getNumChannels());
        for(int s=0;s<b.getNumSamples();++s,++counter) {
            int pos=int(counter%hop),delayPos=int(counter%latency);
            if(pos==0 && counter>=latency) {
                int64_t wanted=(counter-latency)/hop;
                voiceValid=false;
                auto fetch=[&] {
                    while(true) {
                        if(!pendingValid) pendingValid=out.pop(pending);
                        if(!pendingValid) return;
                        if(pending.index<wanted) {pendingValid=false;continue;}
                        if(pending.index==wanted) {current=pending;pendingValid=false;voiceValid=true;}
                        return;
                    }
                };
                fetch();
                // Only offline host rendering may wait. Never wait in the real-time callback.
                if(isNonRealtime()) {
                    for(int wait=0;!voiceValid && !failed && wait<5000;++wait) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1)); fetch();
                    }
                }
                if(!voiceValid) ++underruns;
            }
            float vg=gv.getNextValue(),ng=gn.getNextValue(),w=wet.getNextValue();
            for(int c=0;c<channels;++c) {
                float x=b.getSample(c,s); if(!std::isfinite(x)) x=0;
                incoming.data[c][pos]=x;
                float d=dry[c][delayPos]; dry[c][delayPos]=x;
                float v=voiceValid?current.data[c][pos]:d;
                float processed=v*vg+(d-v)*ng;
                // Missing worker deadline: retain aligned audio, never read a late block.
                b.setSample(c,s,voiceValid && !failed ? d+w*(processed-d) : d);
            }
            if(pos==hop-1) {
                incoming.index=counter/hop;
                if(!failed && !in.push(incoming)) {failed=true;++underruns;}
            }
        }
    }
    void getStateInformation(juce::MemoryBlock& data) override {
        auto xml=state.copyState().createXml(); copyXmlToBinary(*xml,data);
    }
    void setStateInformation(const void* data,int size) override {
        auto xml=getXmlFromBinary(data,size);
        if(xml && xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
    }
    bool hasEditor() const override{return true;}
    juce::AudioProcessorEditor* createEditor() override;
private:
    Queue<128> in,out;
    std::thread worker; std::atomic<bool> running{false};
    std::array<std::array<float,latency>,2> dry{};
    Packet incoming,current,pending;
    int64_t counter=0; bool pendingValid=false,voiceValid=false;
    juce::SmoothedValue<float> gv,gn,wet;
};

class SexyLook : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,float start,float end,juce::Slider& s) override {
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(12);
        float r=juce::jmin(bounds.getWidth(),bounds.getHeight())*.5f;
        auto c=bounds.getCentre(); auto accent=s.findColour(juce::Slider::rotarySliderFillColourId);
        for(int i=0;i<=35;++i) {
            float a=start+(end-start)*i/35;
            g.setColour(i/35.f<=position?accent:juce::Colour(0xff474a50));
            g.drawLine(c.x+std::sin(a)*(r-1),c.y-std::cos(a)*(r-1),c.x+std::sin(a)*(r+4),c.y-std::cos(a)*(r+4),1.6f);
        }
        r-=10;
        g.setColour(juce::Colour(0xff0c0d0f));g.fillEllipse(c.x-r-4,c.y-r-1,2*r+8,2*r+8);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff656669),c.x,c.y-r,juce::Colour(0xff202226),c.x,c.y+r,false));
        g.fillEllipse(c.x-r,c.y-r,2*r,2*r);
        r-=5;g.setColour(juce::Colour(0xff090a0c));g.fillEllipse(c.x-r,c.y-r,2*r,2*r);
        r-=5;g.setGradientFill(juce::ColourGradient(juce::Colour(0xff45474b),c.x,c.y-r,juce::Colour(0xff25272b),c.x,c.y+r,false));
        g.fillEllipse(c.x-r,c.y-r,2*r,2*r);
        float a=start+position*(end-start);
        g.setColour(accent);g.drawLine(c.x+std::sin(a)*(r-9),c.y-std::cos(a)*(r-9),c.x+std::sin(a)*(r-2),c.y-std::cos(a)*(r-2),3.5f);
        g.setColour(juce::Colour(0xffefede9));g.setFont(juce::FontOptions(25.f));
        g.drawText(juce::String(s.getValue(),1),int(c.x-r),int(c.y-23),int(2*r),32,juce::Justification::centred);
        g.setColour(juce::Colour(0xffa1a3a7));g.setFont(juce::FontOptions(10.f));
        g.drawText(s.getName(),int(c.x-r),int(c.y+12),int(2*r),18,juce::Justification::centred);
    }
};

class SexyEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    SexyEditor(SexyProcessor& p):AudioProcessorEditor(p),processor(p) {
        setLookAndFeel(&look); setSize(560,420);
        for(auto* s:{&voice,&noise}) {
            addAndMakeVisible(s);s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            s->setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
            s->setDoubleClickReturnValue(true,0);s->setMouseDragSensitivity(220);
            s->setTooltip("Arraste para ajustar · Shift: ajuste fino · Duplo clique: 0 dB");
        }
        voice.setName("VOICE");noise.setName("NOISE");
        voice.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffe5ad83));
        noise.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff89cbd5));
        voiceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"voice",voice);
        noiseAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"noise",noise);
        addAndMakeVisible(bypass);bypass.setButtonText("BYPASS");
        bypass.setColour(juce::ToggleButton::textColourId,juce::Colour(0xffbebfc3));
        bypass.setColour(juce::ToggleButton::tickColourId,juce::Colour(0xffe5ad83));
        bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);
        startTimerHz(4);
    }
    ~SexyEditor() override {setLookAndFeel(nullptr);}
    void resized() override {voice.setBounds(310,10,182,176);noise.setBounds(310,185,182,176);bypass.setBounds(30,343,135,30);}
    void paint(juce::Graphics& g) override {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff414347),0,0,juce::Colour(0xff17191d),220,420,false));g.fillAll();
        g.setColour(juce::Colour(0xff1b1e23));g.fillRoundedRectangle(225,1,334,418,30);
        g.setColour(juce::Colour(0xff626269));g.drawRoundedRectangle(225,1,334,418,30,1);
        g.saveState();g.reduceClipRegion(juce::Rectangle<int>(228,3,329,412));
        for(int i=0;i<12;++i) {
            juce::Path p;float d=float(i*11);
            p.startNewSubPath(236+d,203);p.lineTo(397,30+d);p.lineTo(550-d,203);p.lineTo(397,390-d);p.closeSubPath();
            g.setColour(juce::Colour(i%2?0xff554238:0xff334b52));g.strokePath(p,juce::PathStrokeType(.8f));
        }
        g.restoreState();
        juce::Path logo;logo.startNewSubPath(28,33);logo.lineTo(39,54);logo.lineTo(51,33);
        g.setColour(juce::Colour(0xffe5ad83));g.strokePath(logo,juce::PathStrokeType(2.5f));
        logo.clear();logo.startNewSubPath(42,33);logo.lineTo(53,54);logo.lineTo(65,33);
        g.setColour(juce::Colour(0xff89cbd5));g.strokePath(logo,juce::PathStrokeType(2.5f));
        g.setColour(juce::Colour(0xfff0eeea));g.setFont(juce::FontOptions(20.f,juce::Font::bold));
        g.drawText("VOICESEX",76,27,142,29,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.f));g.setColour(juce::Colour(0xffb3b3b5));g.drawText("a u d i o",78,55,135,20,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffe3e1dd));g.setFont(juce::FontOptions(26.f,juce::Font::bold));
        g.drawText("SEXYVOX",245,372,290,35,juce::Justification::centred);
        g.setColour(juce::Colour(0xffa3a7ae));g.setFont(juce::FontOptions(11.f));
        g.drawText("VOICE / NOISE",30,137,175,20,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(14.f));g.drawText("A voz em",30,168,180,22,juce::Justification::centredLeft);
        g.drawText("primeiro plano.",30,190,180,22,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffe5ad83));g.drawLine(30,237,74,237,2);
        g.setFont(juce::FontOptions(10.f));
        juce::String status;
        if(!processor.supported) status="Use a sessao em 48 kHz";
        else if(processor.failed) status="Falha no motor · dry ativo";
        else if(!processor.ready) status="Inicializando motor...";
        else if(processor.underruns>0) status="CPU: "+juce::String(processor.underruns.load())+" atrasos · dry";
        else status="REALTIME · 48 kHz";
        g.drawText(status,29,273,193,20,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff91959d));g.drawText("TEST BUILD 0.1.0",30,382,175,18,juce::Justification::centredLeft);
    }
private:
    void timerCallback() override {repaint();}
    SexyProcessor& processor;SexyLook look;juce::Slider voice,noise;juce::ToggleButton bypass;
    juce::TooltipWindow tooltip{this,900};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voiceAttachment,noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
};
juce::AudioProcessorEditor* SexyProcessor::createEditor(){return new SexyEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SexyProcessor();}
int sexyUnderruns(juce::AudioProcessor* p){return static_cast<SexyProcessor*>(p)->underruns.load();}
