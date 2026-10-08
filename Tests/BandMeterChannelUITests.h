#pragma once
namespace {
void testStereoBandMeterDisplay() {
    using namespace pontedsp::mc2000;
    for(int backend:{0,1})for(int mode:{0,1}) {
        PonteMC2000AudioProcessor p;
        set(p,parameters::crossoverMode,float(backend));set(p,parameters::channelMode,float(mode));
        for(int band=0;band<4;++band) {
            set(p,parameters::bandId(band,"thresholdDb"),-30);
            set(p,parameters::bandId(band,"ratio"),4);
            set(p,parameters::bandId(band,"attackMs"),.25f);
        }
        p.prepareToPlay(48000,512);
        auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());offscreenPeer(*editor);
        juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer midi;
        for(int block=0;block<90;++block) {
            for(int sample=0;sample<512;++sample) {
                const double t=double(block*512+sample)/48000;
                float value=0;
                for(double frequency:{60.,500.,5000.,15000.})value+=float(.2*std::sin(2*std::numbers::pi*frequency*t));
                audio.setSample(0,sample,value);audio.setSample(1,sample,value*.1f);
            }
            p.processBlock(audio,midi);
        }
        pump(80);
        std::vector<BandMeter*> meters;
        const auto collect=[&](auto&& self,juce::Component& component)->void {
            if(auto* meter=dynamic_cast<BandMeter*>(&component))meters.push_back(meter);
            for(auto* child:component.getChildren())self(self,*child);
        };
        collect(collect,*editor);
        expect(meters.size()==4,"All four bands expose their channel meters");
        for(auto* meter:meters) {
            const auto values=meter->displayedValues();
            expect(std::abs(values.channelInputDb[0]-values.channelInputDb[1]-20)<.01,
                   "IN displays each channel's actual level in Stereo and Dual Mono");
            if(mode==0) {
                expect(std::abs(values.channelOutputDb[0]-values.channelOutputDb[1]-20)<.01,
                       "OUT retains the actual L/R difference with linked Stereo GR");
                expect(values.channelGainReductionDb[0]==values.channelGainReductionDb[1] && values.channelGainReductionDb[0]>.1,
                       "Stereo shows two equal GR readings from its linked detector");
            } else {
                expect(values.channelGainReductionDb[0]>values.channelGainReductionDb[1]+1,
                       "Dual Mono shows independent channel reduction");
            }
            const auto before=values;
            meter->createComponentSnapshot(meter->getLocalBounds());
            expect(meter->displayedValues()==before,"Painting cannot consume or alter channel readings");
            meter->update(12);
            const auto drained=meter->displayedValues();
            expect(drained.channelInputDb[0]<-99 && drained.channelInputDb[1]<-99
                   && drained.channelOutputDb[0]<-99 && drained.channelOutputDb[1]<-99,
                   "Both channel levels decay when callbacks stop");
        }
        // Refill the meter cache for a meaningful visual review.
        for(int sample=0;sample<512;++sample) {
            const float value=float(.4*std::sin(2*std::numbers::pi*500*sample/48000));
            audio.setSample(0,sample,value);audio.setSample(1,sample,value*.1f);
        }
        p.processBlock(audio,midi);pump(40);
        const auto file=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile(
            juce::String("MC2000_UI_")+(backend?"linear_":"iir_")+(mode?"dual":"stereo")+"_band_meters.png");
        juce::FileOutputStream stream(file);juce::PNGImageFormat png;
        if(stream.openedOk()){stream.setPosition(0);stream.truncate();}
        expect(stream.openedOk() && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),stream),
               "Stereo band meters render at the minimum editor size");
    }
}
}