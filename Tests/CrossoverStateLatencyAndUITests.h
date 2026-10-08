#pragma once
namespace {
void testCrossoverModeStateLatencyAndBypass() {
    using namespace pontedsp::mc2000;
    auto instance = std::make_unique<PonteMC2000AudioProcessor>(); auto& p = *instance;
    auto set=[&](const juce::String& id,float value){auto* param=p.state.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));};
    p.state.copyState();p.undoManager.clearUndoHistory();p.undoManager.beginNewTransaction("Channel mode");
    set(parameters::channelMode,1);p.state.copyState();
    expect(p.undoManager.undo() && p.state.getRawParameterValue(parameters::channelMode)->load()==0,"Channel mode undo restores Stereo");
    expect(p.undoManager.redo() && p.state.getRawParameterValue(parameters::channelMode)->load()==1,"Channel mode redo restores Dual Mono");
    // Existing parameter indices must survive appending the new choices.
    expect(dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[5])->paramID==parameters::crossoverId(0),"Existing host parameter order preserved");
    expect(!p.state.getParameter(parameters::crossoverMode)->isAutomatable(),"Latency-changing mode is not host automatable");
    set(parameters::crossoverMode,1);set(parameters::channelMode,1);p.prepareToPlay(48000,17);
    expect(p.getLatencySamples()==16384,"Wrapper reports FIR and partition delay to host");
    expect(p.getTailLengthSeconds()>.512 && p.getTailLengthSeconds()<.6,"FIR audio tail includes partition output");
    juce::MemoryBlock saved;p.getStateInformation(saved);
    set(parameters::crossoverMode,0);set(parameters::channelMode,0);
    p.setStateInformation(saved.getData(),int(saved.getSize()));
    expect(p.state.getRawParameterValue(parameters::crossoverMode)->load()==1 && p.state.getRawParameterValue(parameters::channelMode)->load()==1,"New choices round trip through session state");
    auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));
    auto old=juce::ValueTree::fromXml(*xml);old.setProperty("schemaVersion",2,nullptr);
    old.removeChild(old.getChildWithProperty("id",parameters::crossoverMode),nullptr);
    old.removeChild(old.getChildWithProperty("id",parameters::channelMode),nullptr);
    auto oldXml=old.createXml();juce::AudioProcessor::copyXmlToBinary(*oldXml,saved);
    p.setStateInformation(saved.getData(),int(saved.getSize()));
    expect(p.state.getRawParameterValue(parameters::crossoverMode)->load()==0 && p.state.getRawParameterValue(parameters::channelMode)->load()==0,"Legacy state loaded over LP/Dual Mono explicitly restores IIR/Stereo");
    expect(p.isCrossoverModePending() && p.getLatencySamples()==16384,"State recall cannot change live PDC behind host");
    p.prepareToPlay(48000,17);expect(p.getLatencySamples()==0 && !p.isCrossoverModePending(),"Host reactivation applies pending mode and latency");
    set(parameters::crossoverMode,1);p.prepareToPlay(48000,17);
    auto* base=static_cast<juce::AudioProcessor*>(&p);juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio(2,257);
    const int latency=p.getLatencySamples();double error=0;
    for(int offset=0;offset<latency+1028;offset+=257) {
        audio.clear();if(offset==0)audio.setSample(1,0,1);
        // Native callback bypass and normal callback alternate with wet history warmed.
        if((offset/257)%2)base->processBlockBypassed(audio,midi);else p.processBlock(audio,midi);
        for(int i=0;i<257;++i) error=std::max(error,std::abs(double(audio.getSample(1,i))-(offset+i==latency?1.:0.)));
    }
    expect(error<=1.e-6,"Bypass retains PDC and return to wet loses no delayed transient, including oversized blocks");
    set(parameters::channelMode,1);
    {
        auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());editor->setSize(1100,738);
        auto* crossover=find<juce::TextButton>(*editor,parameters::crossoverMode);auto* channels=find<juce::TextButton>(*editor,parameters::channelMode);
        expect(crossover && channels && editor->getLocalBounds().contains(crossover->getBounds()) && editor->getLocalBounds().contains(channels->getBounds()),"New selectors fit inside the minimum editor");
        expect(crossover && channels && !crossover->getBounds().intersects(channels->getBounds()),"New selectors cannot overlap");
        auto picture=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("MC2000_UI_linear_dual.png");
        juce::FileOutputStream image(picture);juce::PNGImageFormat png;
        if(image.openedOk()){image.setPosition(0);image.truncate();}
        expect(image.openedOk()&&png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),image),"Linear/Dual Mono minimum editor renders");
    }
    CrossoverPlot plot(p);plot.setSize(500,200);plot.updateSpectrum(.033);
    expect(std::abs(plot.inputResponseDb(1000)) < 1.e-9,"FIR graphical unity sum uses published kernels");
    expect(!find<juce::TextButton>(plot,"crossoverPhaseView"),"Linear Phase has no phase panel or selector");
    set(parameters::crossoverMode,0);expect(p.isCrossoverModePending(),"GUI distinguishes requested and active crossover");
    auto restoredInstance=std::make_unique<PonteMC2000AudioProcessor>(); auto& restored=*restoredInstance;juce::MemoryBlock current;p.getStateInformation(current);restored.setStateInformation(current.getData(),int(current.getSize()));restored.prepareToPlay(96000,64);
    expect(restored.getLatencySamples()==0,"Reopened session applies its saved requested mode");
}
}
