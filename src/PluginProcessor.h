#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "fausts/pitchShifter.cpp"
#include "wasm-ts9.h"
#include "compressor.hpp"
#include <map>
#include <memory>

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

    // =========================================================================
    // GUI-exposed parameters (public so the editor can build attachments)
    // =========================================================================

    // Global toggles
    juce::AudioParameterBool*  useWavFileParam   = nullptr;
    juce::AudioParameterBool*  fuzzEnabledParam  = nullptr;

    // Compressor
    juce::AudioParameterBool*  compEnabledParam  = nullptr;
    juce::AudioParameterFloat* compThreshParam   = nullptr;
    juce::AudioParameterFloat* compRatioParam    = nullptr;
    juce::AudioParameterFloat* compKneeParam     = nullptr;
    juce::AudioParameterFloat* compAttackParam   = nullptr;
    juce::AudioParameterFloat* compReleaseParam  = nullptr;
    juce::AudioParameterFloat* compMakeupParam   = nullptr;

    // Mix levels (dB)
    juce::AudioParameterFloat* downShiftLevelParam = nullptr;
    juce::AudioParameterFloat* upShiftLevelParam   = nullptr;

    // Solo
    juce::AudioParameterBool*  downShiftSoloParam  = nullptr;
    juce::AudioParameterBool*  upShiftSoloParam    = nullptr;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
    
    static juce::AudioProcessor::BusesProperties createBusesProperties();
    static void createTS9ParametersAndInitWasm(juce::AudioProcessor& processor,
                                                w2c_ts9& wasm_app,
                                                wasm_rt_memory_t*& wasm_memory,
                                                std::map<juce::String, int>& parameterIndexMap);
    
    // Audio file playback
    juce::AudioBuffer<float> audioFileBuffer;
    int playbackPosition = 0;
    
    // TS9 WASM module
    w2c_ts9 ts9WasmApp;
    wasm_rt_memory_t* ts9WasmMemory = nullptr;
    std::map<juce::String, int> ts9ParameterIndexMap;
    
    // Pitch shifters (internal params not exposed in GUI)
    mydsp pitchShifterLeft;
    mydsp pitchShifterRight;
    juce::AudioParameterFloat* leftShiftParam    = nullptr;
    juce::AudioParameterFloat* rightShiftParam   = nullptr;
    juce::AudioParameterFloat* leftWindowParam   = nullptr;
    juce::AudioParameterFloat* rightWindowParam  = nullptr;
    juce::AudioParameterFloat* leftXfadeParam    = nullptr;
    juce::AudioParameterFloat* rightXfadeParam   = nullptr;

    // Compressor DSP object
    std::unique_ptr<giml::Compressor<float>> compressor;
};
