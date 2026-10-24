#include <juce_audio_utils/juce_audio_utils.h>
#include "Streaming.h"
#include "BinaryData.h"
#include <atomic>
#include <thread>
#include <chrono>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

namespace {
constexpr int hop=512, latency=2048;
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

template<int Capacity>
struct ResampleFifo {
    std::array<std::array<float, Capacity>, 2> data{};
    int writeIndex = 0;
    int readIndex = 0;
    int numSamples = 0;

    void reset() {
        writeIndex = 0;
        readIndex = 0;
        numSamples = 0;
        for (auto& ch : data) ch.fill(0.f);
    }

    int available() const { return numSamples; }

    void writeSilence(int count, int channels) {
        for (int i = 0; i < count; ++i) {
            for (int c = 0; c < channels; ++c) data[c][writeIndex] = 0.f;
            writeIndex = (writeIndex + 1) % Capacity;
            numSamples = std::min(Capacity, numSamples + 1);
        }
    }

    void push(const float* const* src, int count, int channels) {
        for (int i = 0; i < count; ++i) {
            for (int c = 0; c < channels; ++c) {
                float val = src[c][i];
                data[c][writeIndex] = std::isfinite(val) ? val : 0.f;
            }
            writeIndex = (writeIndex + 1) % Capacity;
            numSamples = std::min(Capacity, numSamples + 1);
        }
    }

    void pushStereo(const float* ch0, const float* ch1, int count, int channels) {
        for (int i = 0; i < count; ++i) {
            data[0][writeIndex] = (ch0 && std::isfinite(ch0[i])) ? ch0[i] : 0.f;
            if (channels > 1) {
                data[1][writeIndex] = (ch1 && std::isfinite(ch1[i])) ? ch1[i] : 0.f;
            }
            writeIndex = (writeIndex + 1) % Capacity;
            numSamples = std::min(Capacity, numSamples + 1);
        }
    }

    int peek(float* dst, int ch, int count) const {
        int toRead = std::min(count, numSamples);
        int r = readIndex;
        for (int i = 0; i < toRead; ++i) {
            dst[i] = data[ch][r];
            r = (r + 1) % Capacity;
        }
        for (int i = toRead; i < count; ++i) dst[i] = 0.f;
        return toRead;
    }

    void advance(int count) {
        int toAdv = std::min(count, numSamples);
        readIndex = (readIndex + toAdv) % Capacity;
        numSamples -= toAdv;
    }
};
}

class SexyProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState state;
    std::atomic<bool> ready{false},failed{false},supported{true};
    std::atomic<int> underruns{0};
    std::atomic<int> failureCode{0};
    SexyProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"SEXYVOX",layout()) {}
    ~SexyProcessor() override { releaseResources(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        for(auto name:{"voice","noise"}) p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{name,1},name,
            juce::NormalisableRange<float>(-50,20,.1f),0,juce::AudioParameterFloatAttributes().withLabel("dB")));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false)); return p;
    }
    const juce::String getName() const override{return "VOICESEX";}
    bool acceptsMidi() const override{return false;} bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override {
        double r = getSampleRate();
        if (r <= 0) r = 48000.0;
        return double(getLatencySamples() + 1024) / r;
    }
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
        supported = (rate >= 22050.0 && rate <= 192000.0);
        failed = false; ready = false; underruns = 0; failureCode = 0;
        in.reset(); out.reset(); counter = 0; pendingValid = false; voiceValid = false;
        incoming = Packet{}; current = Packet{}; for(auto& c:dry) c.fill(0);
        gv.reset(48000, .02); gn.reset(48000, .02); wet.reset(48000, .02);
        gv.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("voice")->load()));
        gn.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(state.getRawParameterValue("noise")->load()));
        wet.setCurrentAndTargetValue(state.getRawParameterValue("bypass")->load()>.5f?0.f:1.f);

        needsResampling = std::abs(rate - 48000.0) >= 0.01;
        ratioIn = rate / 48000.0;
        ratioOut = 48000.0 / rate;

        inFifo.reset();
        outFifo48.reset();
        for (int c = 0; c < 2; ++c) {
            resampIn[c].reset();
            resampOut[c].reset();
        }

        const int channels = std::clamp(getTotalNumInputChannels(), 1, 2);

        if (needsResampling) {
            inFifo.writeSilence(P_in, channels);
            outFifo48.writeSilence(P_out, channels);
            int reportedLatency = P_in + 2 + int(std::round((latency + 2 + P_out) * (rate / 48000.0)));
            setLatencySamples(reportedLatency);
        } else {
            setLatencySamples(latency);
        }

        if(!supported) return;
        std::array<std::unique_ptr<StreamChannel>,2> streams;
        try {
            for(int c=0;c<channels;++c) streams[c]=std::make_unique<StreamChannel>();
        } catch(...) {failureCode=1;failed=true;return;}
        running=true;ready=true;
        worker=std::thread([this,channels,streams=std::move(streams)]() mutable {
            timeBeginPeriod(1);
            try {
                Packet input,output; int64_t expected=0;
                while(running.load(std::memory_order_acquire)) {
                    if(!in.pop(input)) {std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
                    if(input.index!=expected) {failureCode=2;failed=true;break;}
                    ++expected; output.index=input.index-1;
                    for(int c=0;c<channels;++c) streams[c]->process(input.data[c].data(),output.data[c].data());
                    if(output.index>=0) {
                        while(running && !out.push(output)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            } catch(...) {failureCode=4;failed=true; ready=false;}
            timeEndPeriod(1);
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

        if (!needsResampling) {
            for (int s = 0; s < b.getNumSamples(); ++s) {
                float y0 = 0.f, y1 = 0.f;
                process48Sample(b.getSample(0, s), channels > 1 ? b.getSample(1, s) : 0.f, y0, y1, channels);
                b.setSample(0, s, y0);
                if (channels > 1) b.setSample(1, s, y1);
            }
            return;
        }

        // Resampled path
        inFifo.push(b.getArrayOfReadPointers(), b.getNumSamples(), channels);

        int max48Needed = int(std::ceil(b.getNumSamples() * ratioOut)) + 16;
        int targetLevel = P_out + max48Needed;

        if (outFifo48.available() < targetLevel) {
            int kNeeded = targetLevel - outFifo48.available();
            while (kNeeded > 0) {
                int chunkSize = std::min(kNeeded, 1024);
                int maxHostNeeded = int(std::ceil(chunkSize * ratioIn)) + 16;
                int hostUsed = 0;
                for (int c = 0; c < channels; ++c) {
                    inFifo.peek(tempHostIn[c].data(), c, maxHostNeeded);
                    int used = resampIn[c].process(ratioIn, tempHostIn[c].data(), temp48In[c].data(), chunkSize);
                    if (c == 0) hostUsed = used;
                }
                inFifo.advance(hostUsed);

                for (int k = 0; k < chunkSize; ++k) {
                    float y0 = 0.f, y1 = 0.f;
                    process48Sample(temp48In[0][k], channels > 1 ? temp48In[1][k] : 0.f, y0, y1, channels);
                    temp48Out[0][k] = y0;
                    if (channels > 1) temp48Out[1][k] = y1;
                }

                outFifo48.pushStereo(temp48Out[0].data(), temp48Out[1].data(), chunkSize, channels);
                kNeeded -= chunkSize;
            }
        }

        int out48Used = 0;
        for (int c = 0; c < channels; ++c) {
            outFifo48.peek(temp48In[c].data(), c, max48Needed);
            int used = resampOut[c].process(ratioOut, temp48In[c].data(), b.getWritePointer(c), b.getNumSamples());
            if (c == 0) out48Used = used;
        }
        outFifo48.advance(out48Used);
    }
    void getStateInformation(juce::MemoryBlock& data) override {
        if(auto xml=state.copyState().createXml()) copyXmlToBinary(*xml,data);
    }
    void setStateInformation(const void* data,int size) override {
        if(!data || size<=0) return;
        try {
            auto xml=getXmlFromBinary(data,size);
            if(xml) {
                if(xml->hasTagName(state.state.getType()) || xml->hasTagName("PARAMETERS") || xml->hasTagName("AudioProcessorValueTreeState") || xml->hasTagName("SEXYVOX")) {
                    xml->setTagName(state.state.getType());
                    auto vt=juce::ValueTree::fromXml(*xml);
                    if(vt.isValid()) state.replaceState(vt);
                }
            } else {
                juce::MemoryInputStream stream(data,size,false);
                auto vt=juce::ValueTree::readFromStream(stream);
                if(vt.isValid() && (vt.hasType(state.state.getType()) || vt.hasType("PARAMETERS") || vt.hasType("AudioProcessorValueTreeState") || vt.hasType("SEXYVOX"))) {
                    state.replaceState(vt);
                }
            }
        } catch(...) {}
    }
    bool hasEditor() const override{return true;}
    juce::AudioProcessorEditor* createEditor() override;
private:
    void process48Sample(float in0, float in1, float& out0, float& out1, int channels) {
        int pos = int(counter % hop);
        int delayPos = int(counter % latency);
        if (pos == 0 && counter >= latency) {
            int64_t wanted = (counter - latency) / hop;
            voiceValid = false;
            auto fetch = [&] {
                while (true) {
                    if (!pendingValid) pendingValid = out.pop(pending);
                    if (!pendingValid) return;
                    if (pending.index < wanted) { pendingValid = false; continue; }
                    if (pending.index == wanted) { current = pending; pendingValid = false; voiceValid = true; }
                    return;
                }
            };
            fetch();
            // Only offline host rendering may wait. Never wait in the real-time callback.
            if (isNonRealtime()) {
                for (int wait = 0; !voiceValid && !failed && wait < 5000; ++wait) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); fetch();
                }
            }
            if (!voiceValid) ++underruns;
        }
        float vg = gv.getNextValue(), ng = gn.getNextValue(), w = wet.getNextValue();
        float in_c[2] = { in0, in1 };
        float out_c[2] = { in0, in1 };
        for (int c = 0; c < channels; ++c) {
            float x = in_c[c];
            if (!std::isfinite(x)) x = 0.f;
            incoming.data[c][pos] = x;
            float d = dry[c][delayPos];
            dry[c][delayPos] = x;
            float v = voiceValid ? current.data[c][pos] : d;
            float processed = v * vg + (d - v) * ng;
            out_c[c] = (voiceValid && !failed ? d + w * (processed - d) : d);
        }
        if (pos == hop - 1) {
            incoming.index = counter / hop;
            if (!failed && !in.push(incoming)) { failureCode = 3; failed = true; ++underruns; }
        }
        out0 = out_c[0];
        out1 = out_c[1];
        ++counter;
    }

    Queue<128> in, out;
    std::thread worker;
    std::atomic<bool> running{false};
    std::array<std::array<float, latency>, 2> dry{};
    Packet incoming, current, pending;
    int64_t counter = 0;
    bool pendingValid = false, voiceValid = false;
    juce::SmoothedValue<float> gv, gn, wet;

    bool needsResampling = false;
    double ratioIn = 1.0, ratioOut = 1.0;
    static constexpr int P_in = 30, P_out = 30;
    ResampleFifo<16384> inFifo, outFifo48;
    std::array<juce::LagrangeInterpolator, 2> resampIn, resampOut;
    std::array<std::array<float, 8192>, 2> tempHostIn{}, temp48In{}, temp48Out{};
};

class SexyLook : public juce::LookAndFeel_V4 {
public:
    void drawTooltip(juce::Graphics& g,const juce::String& text,int width,int height) override {
        auto accent=juce::Colour(text.startsWith("Voz:")?0xffe5ad83:0xff89cbd5);
        g.setColour(juce::Colour(0xff202329));g.fillRoundedRectangle(0,0,float(width),float(height),6);
        g.setColour(accent.withAlpha(.65f));g.drawRoundedRectangle(.5f,.5f,float(width-1),float(height-1),6,1);
        g.setColour(accent);g.setFont(juce::FontOptions(13.f));
        g.drawFittedText(text,10,7,width-20,height-14,juce::Justification::centredLeft,8,1.f);
    }
    juce::Rectangle<int> getTooltipBounds(const juce::String&,juce::Point<int> screenPos,juce::Rectangle<int> parent) override {
        return juce::Rectangle<int>(screenPos.x+12,screenPos.y+20,320,112).constrainedWithin(parent);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float position,float start,float end,juce::Slider& s) override {
        auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(6);
        float r=juce::jmin(bounds.getWidth(),bounds.getHeight())*.5f;
        auto c=bounds.getCentre(); 
        auto accent=s.findColour(juce::Slider::rotarySliderFillColourId);

        // 1. Outer tick marks ring (brushed indicator ticks)
        for(int i=0;i<=40;++i) {
            float a=start+(end-start)*i/40.0f;
            bool active = (i/40.0f <= position);
            g.setColour(active ? accent.withAlpha(0.95f) : juce::Colour(0xff36383e));
            float len = (i % 5 == 0) ? 6.5f : 4.0f;
            float tW = (i % 5 == 0) ? 1.8f : 1.2f;
            g.drawLine(c.x+std::sin(a)*(r-len), c.y-std::cos(a)*(r-len),
                       c.x+std::sin(a)*r,       c.y-std::cos(a)*r, tW);
        }

        r -= 9.0f;
        // 2. Beveled outer rim (dark gunmetal metal bezel)
        g.setColour(juce::Colour(0xff08090a));
        g.fillEllipse(c.x-r-3, c.y-r-1, 2*r+6, 2*r+6);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff52555c), c.x, c.y-r,
                                               juce::Colour(0xff18191c), c.x, c.y+r, false));
        g.fillEllipse(c.x-r, c.y-r, 2*r, 2*r);

        // 3. Metallic chamfer ring
        r -= 4.0f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff222428), c.x, c.y-r,
                                               juce::Colour(0xff555861), c.x, c.y+r, false));
        g.fillEllipse(c.x-r, c.y-r, 2*r, 2*r);

        // 4. Glowing Neon Arc Track (subtle depth neon glow)
        r -= 5.0f;
        // Track background groove
        juce::Path trackBg;
        trackBg.addCentredArc(c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, start, end, true);
        g.setColour(juce::Colour(0xff141518));
        g.strokePath(trackBg, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Neon Glow Layer 1 (diffuse outer glow)
        if (position > 0.001f) {
            float curAngle = start + position * (end - start);
            juce::Path glowPath;
            glowPath.addCentredArc(c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, start, curAngle, true);
            
            // Soft diffuse glow
            g.setColour(accent.withAlpha(0.25f));
            g.strokePath(glowPath, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Mid glow
            g.setColour(accent.withAlpha(0.60f));
            g.strokePath(glowPath, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Core intense neon line
            g.setColour(accent.brighter(0.35f));
            g.strokePath(glowPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Needle thumb indicator (bright metallic notch)
            g.setColour(juce::Colours::white);
            g.drawLine(c.x + std::sin(curAngle)*(r - 7.0f), c.y - std::cos(curAngle)*(r - 7.0f),
                       c.x + std::sin(curAngle)*(r + 0.5f), c.y - std::cos(curAngle)*(r + 0.5f), 2.2f);
        }

        // 5. Inner Cap Face (dark radial brushed face)
        r -= 8.0f;
        g.setColour(juce::Colour(0xff0d0e11));
        g.fillEllipse(c.x-r-1, c.y-r-1, 2*r+2, 2*r+2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2d32), c.x, c.y-r,
                                               juce::Colour(0xff18191c), c.x, c.y+r, false));
        g.fillEllipse(c.x-r, c.y-r, 2*r, 2*r);

        // 6. Central Value with soft neon glow text & Label
        g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
        juce::String valStr = juce::String(s.getValue(), 1);
        
        // Soft neon shadow behind text
        g.setColour(accent.withAlpha(0.30f));
        g.drawText(valStr, int(c.x-r), int(c.y-22), int(2*r), 30, juce::Justification::centred);
        
        // Crisp foreground value
        g.setColour(accent.brighter(0.40f));
        g.drawText(valStr, int(c.x-r), int(c.y-23), int(2*r), 30, juce::Justification::centred);

        // Name label
        g.setColour(juce::Colour(0xff8c9099));
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(s.getName(), int(c.x-r), int(c.y+11), int(2*r), 18, juce::Justification::centred);
    }
};

class SettingsPopupComponent : public juce::Component {
public:
    std::function<void()> onDismiss;
    SettingsPopupComponent() {
        setAlwaysOnTop(true);
    }
    void paint(juce::Graphics& g) override {
        auto mb = getLocalBounds().toFloat();
        // Drop shadow / glow around menu
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillRoundedRectangle(mb.expanded(1.0f), 7.0f);

        // Dark panel background
        g.setColour(juce::Colour(0xfa16181c));
        g.fillRoundedRectangle(mb, 6.0f);
        g.setColour(juce::Colour(0xff3a3e47));
        g.drawRoundedRectangle(mb, 6.0f, 1.2f);

        // Title
        g.setColour(juce::Colour(0xfff0eeea));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("SETTINGS", 10, 8, getWidth() - 20, 18, juce::Justification::centredLeft);
        
        g.setColour(juce::Colour(0xff33373f));
        g.drawLine(8, 30, getWidth() - 8, 30, 1.0f);

        // Support item (hoverable appearance)
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setColour(hoverSupport ? juce::Colour(0xff89cbd5) : juce::Colour(0xffe3e1dd));
        g.drawText("SUPPORT", 10, 38, getWidth() - 20, 22, juce::Justification::centredLeft);

        // About item
        g.setColour(hoverAbout ? juce::Colour(0xffe5ad83) : juce::Colour(0xffe3e1dd));
        g.drawText("ABOUT", 10, 68, getWidth() - 20, 22, juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff33373f));
        g.drawLine(8, 98, getWidth() - 8, 98, 1.0f);

        // Version string
        g.setFont(juce::FontOptions(9.5f));
        g.setColour(juce::Colour(0xff8a8d94));
        g.drawText("v1.0.0", 0, 104, getWidth(), 18, juce::Justification::centred);
    }

    void mouseMove(const juce::MouseEvent& e) override {
        bool hs = (e.y >= 36 && e.y < 62);
        bool ha = (e.y >= 66 && e.y < 92);
        if (hs != hoverSupport || ha != hoverAbout) {
            hoverSupport = hs; hoverAbout = ha;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (e.y >= 36 && e.y < 62) { // SUPPORT
            juce::URL("https://github.com/neandertalzinhozinho/VOICESEX---Denoiser").launchInDefaultBrowser();
            if (onDismiss) onDismiss();
        } else if (e.y >= 66 && e.y < 92) { // ABOUT
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "About VOICESEX",
                "VOICESEX v1.0.0 by NDERTAL LABS\n\nThanks for using!\nFor more visit:\nhttps://github.com/neandertalzinhozinho\n\nReport any issues at:\nhttps://github.com/neandertalzinhozinho/VOICESEX---Denoiser"
            );
            if (onDismiss) onDismiss();
        }
    }
private:
    bool hoverSupport = false;
    bool hoverAbout = false;
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
        }
        voice.setName("VOICE");noise.setName("NOISE");
        voice.setTooltip("Voice: adjust your vocal presence level.\nDrag to adjust. Hold Shift for fine control or double-click to reset to 0 dB.");
        noise.setTooltip("Noise: set how much background ambience to retain.\nDrag to adjust. Hold Shift for fine control or double-click to reset to 0 dB.");
        tooltip.setLookAndFeel(&look);
        voice.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffe5ad83));
        noise.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff89cbd5));
        voiceAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"voice",voice);
        noiseAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"noise",noise);

        addAndMakeVisible(bypass);bypass.setButtonText("BYPASS");
        bypass.setColour(juce::ToggleButton::textColourId,juce::Colour(0xffbebfc3));
        bypass.setColour(juce::ToggleButton::tickColourId,juce::Colour(0xffe5ad83));
        bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);

        juce::Path gear;
        const int numTeeth = 8;
        const float rInner = 6.0f, rOuter = 10.5f;
        for (int i = 0; i < numTeeth * 2; ++i) {
            float angle = i * juce::MathConstants<float>::pi / float(numTeeth);
            float rad = (i % 2 == 0) ? rOuter : rInner;
            float px = 14.0f + rad * std::cos(angle);
            float py = 14.0f + rad * std::sin(angle);
            if (i == 0) gear.startNewSubPath(px, py);
            else gear.lineTo(px, py);
        }
        gear.closeSubPath();
        gear.addEllipse(10.5f, 10.5f, 7.0f, 7.0f);
        gear.setUsingNonZeroWinding(false);
        settingsBtn.setShape(gear, false, true, true);
        settingsBtn.setColours(juce::Colour(0xff8a8d94), juce::Colour(0xffe5ad83), juce::Colour(0xfff0eeea));

        addAndMakeVisible(settingsBtn);
        settingsBtn.setTooltip("Settings & Info");
        settingsBtn.onClick = [this]() {
            bool shouldShow = !settingsPopup.isVisible();
            if (shouldShow) {
                settingsPopup.setVisible(true);
                settingsPopup.toFront(true);
            } else {
                settingsPopup.setVisible(false);
            }
        };

        settingsPopup.onDismiss = [this]() {
            settingsPopup.setVisible(false);
        };
        addChildComponent(settingsPopup);

        if (BinaryData::logo_ndertal_png != nullptr && BinaryData::logo_ndertal_pngSize > 0) {
            logoImg = juce::ImageFileFormat::loadFrom(BinaryData::logo_ndertal_png, BinaryData::logo_ndertal_pngSize);
        }

        startTimerHz(4);
    }
    ~SexyEditor() override {tooltip.setLookAndFeel(nullptr);setLookAndFeel(nullptr);}
    void resized() override {
        // Centro do painel decorativo x = 392 (225 + 334/2)
        // Largura dos knobs: 172 px -> x = 392 - 86 = 306
        voice.setBounds(306, 12, 172, 172);
        noise.setBounds(306, 192, 172, 172);
        bypass.setBounds(30, 343, 135, 30);
        settingsBtn.setBounds(518, 14, 28, 28);
        settingsPopup.setBounds(444, 46, 104, 126);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (settingsPopup.isVisible()) {
            if (!settingsPopup.getBounds().contains(e.getPosition()) && !settingsBtn.getBounds().contains(e.getPosition())) {
                settingsPopup.setVisible(false);
            }
        }
    }

    void paint(juce::Graphics& g) override {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff414347),0,0,juce::Colour(0xff17191d),220,420,false));g.fillAll();
        g.setColour(juce::Colour(0xff1b1e23));g.fillRoundedRectangle(225,1,334,418,30);
        g.setColour(juce::Colour(0xff626269));g.drawRoundedRectangle(225,1,334,418,30,1);
        g.saveState();g.reduceClipRegion(juce::Rectangle<int>(228,3,329,412));
        for(int i=0;i<12;++i) {
            juce::Path p;float d=float(i*11);
            p.startNewSubPath(236+d,203);p.lineTo(392,30+d);p.lineTo(548-d,203);p.lineTo(392,376-d);p.closeSubPath();
            g.setColour(juce::Colour(i%2?0xff554238:0xff334b52));g.strokePath(p,juce::PathStrokeType(.8f));
        }
        g.restoreState();

        // NDERTAL LABS High-res Bronze Logo
        if (logoImg.isValid()) {
            g.drawImage(logoImg, 18, 18, 54, 54, 0, 0, logoImg.getWidth(), logoImg.getHeight());
        }

        g.setColour(juce::Colour(0xfff0eeea));g.setFont(juce::FontOptions(20.f,juce::Font::bold));
        g.drawText("VOICESEX",76,26,145,26,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));g.setColour(juce::Colour(0xff89cbd5));
        g.drawText("NDERTAL LABS",77,50,145,18,juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xffe3e1dd));g.setFont(juce::FontOptions(26.f,juce::Font::bold));
        g.drawText("SEXYVOX",245,372,290,35,juce::Justification::centred);
        g.setColour(juce::Colour(0xffa3a7ae));g.setFont(juce::FontOptions(11.f));
        g.drawText("VOICE / NOISE",30,137,175,20,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(14.f));
        g.drawText("The voice in the",30,168,180,22,juce::Justification::centredLeft);
        g.drawText("foreground.",30,190,180,22,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffe5ad83));g.drawLine(30,237,74,237,2);

        g.setFont(juce::FontOptions(10.f));
        juce::String status;
        if(!processor.supported) status="Unsupported sample rate";
        else if(processor.failed) status="Engine error ("+juce::String(processor.failureCode.load())+")";
        else if(!processor.ready) status="Initializing engine...";
        else if(processor.underruns>0) status="Registered delays: "+juce::String(processor.underruns.load());
        else if(std::abs(processor.getSampleRate()-48000.0)<0.01) status="REALTIME  48 kHz  42.7 ms";
        else if(std::abs(processor.getSampleRate()-44100.0)<0.01) {
            int latMs = juce::roundToInt(processor.getLatencySamples() * 1000.0 / processor.getSampleRate());
            status="REALTIME  44.1 kHz  "+juce::String(latMs)+" ms ("+juce::String(processor.getLatencySamples())+" smp)";
        }
        else status="REALTIME  "+juce::String(processor.getSampleRate()/1000.0,1)+" kHz";
        g.setColour(processor.underruns > 0 ? juce::Colour(0xffe5ad83) : juce::Colour(0xff91959d));
        g.drawText(status,29,273,193,20,juce::Justification::centredLeft);
    }
private:
    void timerCallback() override {repaint();}
    SexyProcessor& processor;SexyLook look;juce::Slider voice,noise;juce::ToggleButton bypass;
    juce::TooltipWindow tooltip{this,900};
    juce::ShapeButton settingsBtn{"Settings", juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::transparentBlack};
    SettingsPopupComponent settingsPopup;
    juce::Image logoImg;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voiceAttachment,noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
};
juce::AudioProcessorEditor* SexyProcessor::createEditor(){return new SexyEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SexyProcessor();}
int sexyUnderruns(juce::AudioProcessor* p){return static_cast<SexyProcessor*>(p)->underruns.load();}
