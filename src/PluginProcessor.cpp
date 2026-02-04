#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
// #include "wasm-app.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

//==============================================================================
juce::AudioProcessor::BusesProperties AudioPluginAudioProcessor::createBusesProperties()
{
    return BusesProperties()
        #if ! JucePlugin_IsMidiEffect
         #if ! JucePlugin_IsSynth
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
         #endif
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        #endif
        ;
}

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (createBusesProperties())
{    
    // Load the WAV file from binary data
    std::cout << "Loading audio file from binary data..." << std::endl;
    
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource("RawGTR_wav", dataSize);
    
    if (data != nullptr && dataSize > 0)
    {
        std::cout << "Binary data found: " << dataSize << " bytes" << std::endl;
        
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        
        auto inputStream = std::make_unique<juce::MemoryInputStream>(data, dataSize, false);
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(std::move(inputStream)));
        
        if (reader != nullptr)
        {
            std::cout << "Audio file loaded successfully!" << std::endl;
            std::cout << "  Sample rate: " << reader->sampleRate << std::endl;
            std::cout << "  Num channels: " << reader->numChannels << std::endl;
            std::cout << "  Length in samples: " << reader->lengthInSamples << std::endl;
            std::cout << "  Duration: " << (reader->lengthInSamples / reader->sampleRate) << " seconds" << std::endl;
            
            audioFileBuffer.setSize(reader->numChannels, (int)reader->lengthInSamples);
            reader->read(&audioFileBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
            
            std::cout << "Audio file loaded into buffer" << std::endl;
        }
        else
        {
            std::cout << "ERROR: Could not create audio reader!" << std::endl;
        }
    }
    else
    {
        std::cout << "ERROR: Binary data not found!" << std::endl;
    }
    
    // Create parameters
    parameterGroup = std::make_unique<juce::AudioProcessorParameterGroup>("parameters", "Parameters", "|");
    
    leftShiftParam = new juce::AudioParameterFloat("leftShift", "Left Shift (semitones)", -12.0f, 12.0f, -12.0f);
    rightShiftParam = new juce::AudioParameterFloat("rightShift", "Right Shift (semitones)", -12.0f, 12.0f, 12.0f);
    leftWindowParam = new juce::AudioParameterFloat("leftWindow", "Left Window (samples)", 50.0f, 10000.0f, 1000.0f);
    rightWindowParam = new juce::AudioParameterFloat("rightWindow", "Right Window (samples)", 50.0f, 10000.0f, 1000.0f);
    leftXfadeParam = new juce::AudioParameterFloat("leftXfade", "Left Xfade (samples)", 1.0f, 10000.0f, 10.0f);
    rightXfadeParam = new juce::AudioParameterFloat("rightXfade", "Right Xfade (samples)", 1.0f, 10000.0f, 10.0f);
    
    parameterGroup->addChild(std::unique_ptr<juce::AudioParameterFloat>(leftShiftParam));
    parameterGroup->addChild(std::unique_ptr<juce::AudioParameterFloat>(rightShiftParam));
    parameterGroup->addChild(std::unique_ptr<juce::AudioParameterFloat>(leftWindowParam));
    parameterGroup->addChild(std::unique_ptr<juce::AudioParameterFloat>(rightWindowParam));
    parameterGroup->addChild(std::unique_ptr<juce::AudioParameterFloat>(leftXfadeParam));
    parameterGroup->addChild(std::unique_ptr<juce::AudioParameterFloat>(rightXfadeParam));
    
    addParameterGroup(std::move(parameterGroup));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    
    // Initialize pitch shifters
    pitchShifterLeft.init(static_cast<int>(sampleRate));
    pitchShifterRight.init(static_cast<int>(sampleRate));
    
    // Set pitch shift parameters from current parameter values
    pitchShifterLeft.fHslider1 = *leftShiftParam;    // shift (semitones)
    pitchShifterLeft.fHslider0 = *leftWindowParam;   // window (samples)
    pitchShifterLeft.fHslider2 = *leftXfadeParam;    // xfade (samples)
    
    pitchShifterRight.fHslider1 = *rightShiftParam;  // shift (semitones)
    pitchShifterRight.fHslider0 = *rightWindowParam; // window (samples)
    pitchShifterRight.fHslider2 = *rightXfadeParam;  // xfade (samples)
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Simple audio file playback with pitch shifting
    if (audioFileBuffer.getNumSamples() > 0)
    {
        int numSamples = buffer.getNumSamples();
        int numChannels = totalNumOutputChannels;
        int fileChannels = audioFileBuffer.getNumChannels();

        // Temporary buffers for pitch shifting
        juce::AudioBuffer<float> tempBuffer(1, numSamples); // FAUST processes mono
        juce::AudioBuffer<float> originalBuffer(1, numSamples); // Store original audio
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* outputData = buffer.getWritePointer(channel);
            
            // Read from the appropriate channel of the audio file
            int fileChannel = std::min(channel, fileChannels - 1);
            const float* fileData = audioFileBuffer.getReadPointer(fileChannel);
            
            // Fill temp buffer with file data (for processing) and original buffer
            float* tempData = tempBuffer.getWritePointer(0);
            float* originalData = originalBuffer.getWritePointer(0);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                int pos = playbackPosition + sample;
                if (pos >= audioFileBuffer.getNumSamples())
                    pos %= audioFileBuffer.getNumSamples(); // loop
                
                float sampleValue = fileData[pos];
                tempData[sample] = sampleValue;
                originalData[sample] = sampleValue;
            }
            
            // Update pitch shifter parameters from current parameter values
            pitchShifterLeft.fHslider1 = *leftShiftParam;    // shift (semitones)
            pitchShifterLeft.fHslider0 = *leftWindowParam;   // window (samples)
            pitchShifterLeft.fHslider2 = *leftXfadeParam;    // xfade (samples)
            
            pitchShifterRight.fHslider1 = *rightShiftParam;  // shift (semitones)
            pitchShifterRight.fHslider0 = *rightWindowParam; // window (samples)
            pitchShifterRight.fHslider2 = *rightXfadeParam;  // xfade (samples)
            
            // Apply pitch shifting
            float* inputOutputPtr[1] = {tempData};
            
            if (channel == 0) // Left channel
            {
                pitchShifterLeft.compute(numSamples, inputOutputPtr, inputOutputPtr);
            }
            else if (channel == 1) // Right channel
            {
                pitchShifterRight.compute(numSamples, inputOutputPtr, inputOutputPtr);
            }
            
            // Add processed data to original data (additive processing)
            for (int sample = 0; sample < numSamples; ++sample)
            {
                outputData[sample] = originalData[sample] + tempData[sample];
            }
        }

        // Advance playback position
        playbackPosition += numSamples;
        if (playbackPosition >= audioFileBuffer.getNumSamples())
            playbackPosition %= audioFileBuffer.getNumSamples();
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
