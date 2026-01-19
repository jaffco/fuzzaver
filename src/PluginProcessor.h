#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "wasm-app.h"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
    
    static juce::AudioProcessor::BusesProperties createBusesProperties();
    static void createParametersAndInitWasm(juce::AudioProcessor& processor, 
                                           w2c_muff& wasm_app,
                                           wasm_rt_memory_t*& wasm_memory,
                                           std::map<juce::String, int>& parameterIndexMap);
    
    w2c_muff wasm_app;
    wasm_rt_memory_t* wasm_memory = nullptr;
    
    // Map JUCE parameter IDs to WASM parameter indices
    std::map<juce::String, int> parameterIndexMap;
    
    // Audio file playback
    juce::AudioBuffer<float> audioFileBuffer;
    int playbackPosition = 0;
    
    // Bypass parameter
    juce::AudioParameterBool* bypassParameter = nullptr;
};
