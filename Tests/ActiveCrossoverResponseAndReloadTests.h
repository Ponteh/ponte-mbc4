#pragma once
#include <thread>
#include <atomic>
namespace {
void testActiveIirResponse()
{
    using namespace pontedsp::mc2000;
    for (double rate : {44100.,48000.,96000.,192000.}) for (int bands : {2,3,4})
    {
        PonteMC2000AudioProcessor p;
        set(p,parameters::bandCount,float(bands-2)); p.prepareToPlay(rate,64);
        CrossoverPlot plot(p); plot.setSize(565,202); plot.updateSpectrum(.033);
        expect(!find<juce::TextButton>(plot,"crossoverPhaseView"),"The rejected phase view is absent");
        dsp::CrossoverResponse active; unsigned version=0;
        expect(p.getEngine().copyIirResponse(active,version),"Active audio coefficients publish a coherent IIR snapshot");
        double maxError=0;
        for (int point=0;point<CrossoverPlot::curvePointCount;++point)
            for (int band=0;band<bands;++band)
            {
                const auto h=active.bandResponse(band,plot.curvePointFrequency(point));
                maxError=std::max(maxError,std::abs(plot.displayedMagnitudeDb(band,point)-juce::Decibels::gainToDecibels(std::abs(h),-160.0)));
            }
        expect(maxError<1.e-9,"Every plotted IIR point uses the actual applied coefficients");
        set(p,parameters::crossoverId(0),500);
        juce::AudioBuffer<float> audio(2,1);audio.clear();juce::MidiBuffer midi;p.processBlock(audio,midi);
        unsigned changed=0;dsp::CrossoverResponse smoothed;
        expect(p.getEngine().copyIirResponse(smoothed,changed)&&changed!=version&&smoothed.frequencies[0]>100&&smoothed.frequencies[0]<500,
               "Snapshot follows applied smoothing, not the requested destination");
        plot.updateSpectrum(.033);
        expect(std::abs(plot.bandResponse(0,130)-smoothed.bandResponse(0,130))<1.e-12,
               "UI follows the exact coefficients applied in the last audio block");
    }
}

void testMeasuredAudioAgainstIirPlot()
{
    using namespace pontedsp::mc2000;
    const auto file=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory()
        .getChildFile("iir-ui-measured-response.csv");
    juce::FileOutputStream csv(file);
    expect(csv.openedOk(),"Independent audio response evidence can be written");
    if(csv.openedOk()){csv.setPosition(0);csv.truncate();csv.writeText("band,frequency,plot_db,measured_db,error_db\n",false,false,nullptr);}
    double maxError=0;
    for(int band=0;band<4;++band)
    {
        PonteMC2000AudioProcessor p;
        for(int b=0;b<4;++b){set(p,parameters::bandId(b,"ratio"),1);set(p,parameters::bandId(b,"solo"),b==band?1:0);}
        p.prepareToPlay(48000,512);
        CrossoverPlot plot(p);plot.setSize(565,202);
        juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
        // Settle the SOLO ramps before exciting the linear audio path.
        for(int block=0;block<32;++block){audio.clear();p.processBlock(audio,midi);}
        constexpr int points=33;
        std::array<std::complex<double>,points> measured{},oscillator{},increment{};
        for(int i=0;i<points;++i){oscillator[std::size_t(i)]=1;increment[std::size_t(i)]=std::polar(1.0,-2*std::numbers::pi*plot.curvePointFrequency(i*32)/48000);}
        for(int block=0;block<32;++block)
        {
            audio.clear();if(block==0){audio.setSample(0,0,1);audio.setSample(1,0,1);}
            p.processBlock(audio,midi);
            for(int sample=0;sample<512;++sample) for(int i=0;i<points;++i)
            {
                measured[std::size_t(i)]+=double(audio.getSample(0,sample))*oscillator[std::size_t(i)];
                oscillator[std::size_t(i)]*=increment[std::size_t(i)];
            }
        }
        plot.updateSpectrum(.033);
        for(int i=0;i<points;++i)
        {
            const double magnitude=std::abs(measured[std::size_t(i)]);
            const double predicted=plot.displayedMagnitudeDb(band,i*32);
            const double actual=juce::Decibels::gainToDecibels(magnitude,-160.0);
            const double error=std::abs(predicted-actual);
            if(magnitude>=1.e-4)maxError=std::max(maxError,error);
            else expect(std::abs(std::pow(10.0,predicted/20.0)-magnitude)<1.e-7,"Below -80 dB compare absolute response error");
            if(csv.openedOk())csv.writeText(juce::String(band+1)+","+juce::String(plot.curvePointFrequency(i*32),6)+","
                +juce::String(predicted,9)+","+juce::String(actual,9)+","+juce::String(error,9)+"\n",false,false,nullptr);
        }
    }
    expect(maxError<.01,"IIR plotted magnitudes agree with separately measured wrapper audio within 0.01 dB above -80 dB");
    std::cout<<"Independent IIR audio/plot maximum error above -80 dB: "<<maxError<<" dB\n";
}

void testMockupLayoutAndLink()
{
    using namespace pontedsp::mc2000;
    PonteMC2000AudioProcessor p;p.prepareToPlay(48000,512);
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());offscreenPeer(*editor);
    auto* linear=find<juce::TextButton>(*editor,parameters::crossoverMode);
    auto* dual=find<juce::TextButton>(*editor,parameters::channelMode);
    auto* mode=find<juce::ComboBox>(*editor,parameters::bandCount);
    auto* output=find<OutputMeter>(*editor);
    expect(linear&&dual&&mode&&output,"Mockup mode controls and output meter exist");
    for(const auto size:{juce::Point<int>(1100,738),juce::Point<int>(1330,950),juce::Point<int>(1600,1100)})
        for(int count:{2,3,4})
        {
            editor->setSize(size.x,size.y);set(p,parameters::bandCount,float(count-2));pump(40);
            expect(linear&&dual&&mode&&output&&linear->getBounds().getCentreY()==dual->getBounds().getCentreY()
                && mode->getBounds().getCentreY()==dual->getBounds().getCentreY(),"MODE, Linear Phase and Dual Mono share the mockup header row");
            expect(linear&&dual&&output&&!linear->getBounds().intersects(dual->getBounds())
                && dual->getRight()<output->getX(),"Header buttons cannot overlap each other or MAIN OUTPUT");
            expect(!find<juce::TextButton>(*editor,"crossoverPhaseView"),"No phase panel selector in the editor");
        }
    for(int band=0;band<4;++band)
    {
        auto* link=find<juce::TextButton>(*editor,parameters::bandId(band,"link"));
        auto* in=find<juce::TextButton>(*editor,parameters::bandId(band,"enabled"));
        auto* solo=find<juce::TextButton>(*editor,parameters::bandId(band,"solo"));
        expect(link&&in&&solo&&in->getY()==solo->getY()&&link->getY()>in->getY(),"Every band has adjacent IN/SOLO with LINK below");
        if(link){link->triggerClick();pump(40);}
        expect(p.state.getRawParameterValue(parameters::linkMaster)->load()==float(band+1),"Band LINK selects the existing persisted master parameter");
        if(link){link->triggerClick();pump(40);}
        expect(p.state.getRawParameterValue(parameters::linkMaster)->load()==0,"Clicking the selected LINK master unlinks");
    }
    if(dual){dual->triggerClick();pump(40);}
    expect(p.state.getRawParameterValue(parameters::channelMode)->load()==1,"Mockup DUAL MONO button selects Dual Mono");
    if(dual){dual->triggerClick();pump(40);}
    expect(p.state.getRawParameterValue(parameters::channelMode)->load()==0,"DUAL MONO off restores Stereo");
}

// Some hosts prepare synchronously from a latency notification, including when
// automation has already requested a different mode. No audio thread is needed
// to reproduce recursive acquisition of the processor's configuration mutex.
void testSynchronousHostPreparation() {
    using namespace pontedsp::mc2000;
    PonteMC2000AudioProcessor p;p.prepareToPlay(48000,512);
    struct Host final : juce::AudioProcessorListener {
        explicit Host(PonteMC2000AudioProcessor& p):processor(p){}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override {
            if(!details.latencyChanged)return;
            ++changes;
            if(changes==1)set(processor,parameters::crossoverMode,0);
            try {processor.prepareToPlay(48000,512);}
            catch(const std::exception& error) {
                threw=true;
                std::cerr << "Synchronous host preparation: " << error.what() << '\n';
            }
        }
        PonteMC2000AudioProcessor& processor;int changes{};bool threw{};
    } host(p);
    p.addListener(&host);
    set(p,parameters::bandId(1,"gainDb"),-3);
    set(p,parameters::crossoverMode,1);
    p.prepareToPlay(48000,512);
    p.removeListener(&host);
    expect(!host.threw && host.changes==2,
           "host can synchronously prepare through nested latency notifications");
    expect(p.getActiveCrossoverMode()==0 && p.getLatencySamples()==0
           && !p.isPreparing() && !p.isCrossoverModePending()
           && p.getReloadState()==PonteMC2000AudioProcessor::ReloadState::idle,
           "nested host preparation retains the final requested mode and latency");
    expect(p.getEngine().getParameters().bands[1].gainDb==-3,
           "nested host preparation retains the user's band settings");
}

struct ReloadHostObserver : juce::AudioProcessorListener {
    explicit ReloadHostObserver(PonteMC2000AudioProcessor& p):processor(p),messageThread(std::this_thread::get_id()){}
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
    void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override {
        if(!details.latencyChanged)return;
        ++latencyChanges;onMessageThread=onMessageThread && std::this_thread::get_id()==messageThread;
        if(reactivate)processor.prepareToPlay(48000,512);
    }
    PonteMC2000AudioProcessor& processor;std::thread::id messageThread;
    std::atomic<int> latencyChanges {};std::atomic<bool> onMessageThread {true},reactivate {};
};
void testAutomaticCrossoverReload() {
    using namespace pontedsp::mc2000;
    PonteMC2000AudioProcessor p;p.prepareToPlay(48000,512);
    ReloadHostObserver host(p);p.addListener(&host);
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());offscreenPeer(*editor);
    auto* header=find<ContextHeader>(*editor);
    set(p,parameters::bandId(1,"gainDb"),-3);
    std::atomic<bool> stop{},finite{true},nativeBypass{};std::atomic<double> maxStep{};
    std::atomic<std::uint64_t> allocations{},frees{},locks{},waits{},io{};
#if defined(MC2000_TECHNICAL_TESTS)
    realtimeAudit::installImportHooks();
#endif
    std::thread audio([&]{
        juce::AudioBuffer<float> buffer(2,512);juce::MidiBuffer midi;int frame=0;float previous=0;
        while(!stop.load()){
            for(int i=0;i<512;++i){const float sample=float(.2*std::sin(2*std::numbers::pi*100*(frame+i)/48000));buffer.setSample(0,i,sample);buffer.setSample(1,i,sample*.1f);}
#if defined(MC2000_TECHNICAL_TESTS)
            {realtimeAudit::Guard guard;if(nativeBypass.load())static_cast<juce::AudioProcessor*>(&p)->processBlockBypassed(buffer,midi);else p.processBlock(buffer,midi);}
            allocations+=realtimeAudit::counts.allocations;frees+=realtimeAudit::counts.frees;locks+=realtimeAudit::counts.locks;waits+=realtimeAudit::counts.waits;io+=realtimeAudit::counts.io;
#else
            if(nativeBypass.load())static_cast<juce::AudioProcessor*>(&p)->processBlockBypassed(buffer,midi);else p.processBlock(buffer,midi);
#endif
            double peak=maxStep.load();
            for(int i=0;i<512;++i){const float sample=buffer.getSample(0,i);if(!std::isfinite(sample))finite.store(false);peak=std::max(peak,std::abs(double(sample)-previous));previous=sample;}
            maxStep.store(peak);frame+=512;std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    const auto finish=[&](int mode){
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){pump(5);if(p.getActiveCrossoverMode()==mode && !p.isCrossoverModePending() && p.getReloadState()==PonteMC2000AudioProcessor::ReloadState::idle)return true;}
        return false;
    };
    auto* modeButton=find<juce::TextButton>(*editor,parameters::crossoverMode);
    expect(modeButton!=nullptr,"Crossover mode is a one-click toggle");
    if(modeButton)modeButton->triggerClick();
    bool sawLoading=false,sawHeader=false;
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
    while(juce::Time::getMillisecondCounterHiRes()<deadline){
        pump(5);sawLoading=sawLoading||p.getCrossoverReloadMessage().isNotEmpty();sawHeader=sawHeader||(header&&header->isShowingStatus());
        if(p.getActiveCrossoverMode()==1 && !p.isCrossoverModePending() && p.getReloadState()==PonteMC2000AudioProcessor::ReloadState::idle)break;
    }
    expect(p.getActiveCrossoverMode()==1 && p.getLatencySamples()==16384 && sawLoading && sawHeader,"Selecting Linear Phase automatically loads DSP and reports progress in the existing header");
    expect(maxStep.load()<.03,"Automatic switch uses controlled fade-out and warm fade-in without a full-level step");
    host.reactivate=true;nativeBypass.store(true);
    set(p,parameters::crossoverMode,0);pump(40);set(p,parameters::crossoverMode,1);pump(5);set(p,parameters::crossoverMode,0);
    expect(finish(0)&&p.getLatencySamples()==0,"Rapid mode requests converge and synchronous host reactivation does not deadlock");
    stop.store(true);audio.join();p.removeListener(&host);
    expect(finite.load() && allocations==0 && frees==0 && locks==0 && waits==0 && io==0,"Callbacks remain finite and free of allocation, locks, waits and I/O during reload");
    expect(host.latencyChanges>=2&&host.onMessageThread,"Latency changes notify the host on the message thread");
    expect(p.state.getRawParameterValue(parameters::bandId(1,"gainDb"))->load()==-3,"Crossover reload preserves the user's band settings");
    const auto readyWithoutAudio=[&](int mode){
        const auto end=juce::Time::getMillisecondCounterHiRes()+10000;
        while(juce::Time::getMillisecondCounterHiRes()<end){pump(5);if(p.getActiveCrossoverMode()==mode&&!p.isCrossoverModePending()&&!p.isPreparing()&&p.getCrossoverReloadMessage().isEmpty())return true;}
        return false;
    };
    set(p,parameters::crossoverMode,1);
    pump(110);editor.reset();editor.reset(p.createEditor());offscreenPeer(*editor);
    expect(readyWithoutAudio(1)&&p.getLatencySamples()==16384,"Mode loads with stopped transport and a recreated editor without waiting for callbacks");
    set(p,parameters::crossoverMode,0);
    expect(readyWithoutAudio(0)&&p.getLatencySamples()==0,"A second stopped-transport request is not blocked by FIR warm-up");
    pump(80); // Let the editor consume the final active snapshot before capturing.
    CrossoverPlot plot(p);plot.setSize(565,202);plot.updateSpectrum(.033);
    const auto file=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("MC2000_UI_iir_magnitude.png");
    juce::FileOutputStream image(file);if(image.openedOk()){image.setPosition(0);image.truncate();juce::PNGImageFormat png;expect(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),image),"IIR magnitude minimum UI renders");}
}
}