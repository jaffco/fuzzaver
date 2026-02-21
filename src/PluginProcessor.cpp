#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

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
    // Initialize TS9 WASM module
    createTS9ParametersAndInitWasm(*this, ts9WasmApp, ts9WasmMemory, ts9ParameterIndexMap);
    
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
    addParameter(useWavFileParam = new juce::AudioParameterBool(
        juce::ParameterID{"useWavFile", 1}, "Use WAV File", false));
    addParameter(fuzzEnabledParam = new juce::AudioParameterBool(
        juce::ParameterID{"fuzzEnabled", 1}, "Fuzz Enabled", false));

    // Internal pitch-shifter parameters (not shown in GUI)
    addParameter(leftShiftParam = new juce::AudioParameterFloat("leftShift", "Left Shift (semitones)", -12.0f, 12.0f, -12.0f));
    addParameter(rightShiftParam = new juce::AudioParameterFloat("rightShift", "Right Shift (semitones)", -12.0f, 12.0f, 12.0f));
    addParameter(leftWindowParam = new juce::AudioParameterFloat("leftWindow", "Left Window (samples)", 50.0f, 10000.0f, 2500.0f));
    addParameter(rightWindowParam = new juce::AudioParameterFloat("rightWindow", "Right Window (samples)", 50.0f, 10000.0f, 2500.0f));
    addParameter(leftXfadeParam = new juce::AudioParameterFloat("leftXfade", "Left Xfade (samples)", 1.0f, 10000.0f, 1500.0f));
    addParameter(rightXfadeParam = new juce::AudioParameterFloat("rightXfade", "Right Xfade (samples)", 1.0f, 10000.0f, 1500.0f));

    // Compressor parameters (first in signal chain)
    addParameter(compEnabledParam = new juce::AudioParameterBool(
        juce::ParameterID{"compEnabled", 1}, "Comp Enabled", true));
    addParameter(compThreshParam = new juce::AudioParameterFloat(
        juce::ParameterID{"compThresh", 1}, "Comp Threshold (dB)",
        juce::NormalisableRange<float>(-60.0f, 0.0f), -20.0f));
    addParameter(compRatioParam = new juce::AudioParameterFloat(
        juce::ParameterID{"compRatio", 1}, "Comp Ratio",
        juce::NormalisableRange<float>(1.1f, 20.0f), 4.0f));
    addParameter(compKneeParam = new juce::AudioParameterFloat(
        juce::ParameterID{"compKnee", 1}, "Comp Knee (dB)",
        juce::NormalisableRange<float>(0.01f, 10.0f), 1.0f));
    addParameter(compAttackParam = new juce::AudioParameterFloat(
        juce::ParameterID{"compAttack", 1}, "Comp Attack (ms)",
        juce::NormalisableRange<float>(0.0f, 100.0f), 3.5f));
    addParameter(compReleaseParam = new juce::AudioParameterFloat(
        juce::ParameterID{"compRelease", 1}, "Comp Release (ms)",
        juce::NormalisableRange<float>(0.0f, 300.0f), 100.0f));
    addParameter(compMakeupParam = new juce::AudioParameterFloat(
        juce::ParameterID{"compMakeup", 1}, "Comp Makeup (dB)",
        juce::NormalisableRange<float>(-20.0f, 20.0f), 0.0f));

    // Mix level parameters (dB, -24 to +6)
    addParameter(downShiftLevelParam = new juce::AudioParameterFloat(
        juce::ParameterID{"downShiftLevel", 1}, "Down Shift Level (dB)",
        juce::NormalisableRange<float>(-24.0f, 6.0f), 4.0f));
    addParameter(upShiftLevelParam = new juce::AudioParameterFloat(
        juce::ParameterID{"upShiftLevel", 1}, "Up Shift Level (dB)",
        juce::NormalisableRange<float>(-24.0f, 6.0f), -8.0f));

    // Solo parameters
    addParameter(downShiftSoloParam = new juce::AudioParameterBool(
        juce::ParameterID{"downShiftSolo", 1}, "Down Shift Solo", false));
    addParameter(upShiftSoloParam = new juce::AudioParameterBool(
        juce::ParameterID{"upShiftSolo", 1}, "Up Shift Solo", false));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    wasm2c_ts9_free(&ts9WasmApp);
}

//==============================================================================
void AudioPluginAudioProcessor::createTS9ParametersAndInitWasm(juce::AudioProcessor& processor,
                                                                w2c_ts9& wasm_app,
                                                                wasm_rt_memory_t*& wasm_memory,
                                                                std::map<juce::String, int>& parameterIndexMap)
{
    std::cout << "=== TS9 WASM Initialization ===" << std::endl;
    
    // Initialize WASM runtime and module
    wasm_rt_init();
    wasm2c_ts9_instantiate(&wasm_app, wasm_app.w2c_env_instance);
    wasm_memory = w2c_ts9_memory(&wasm_app);
    
    std::cout << "WASM memory size: " << wasm_memory->size << " bytes" << std::endl;
    
    // Read JSON metadata from WASM memory BEFORE calling init
    const char* json_cstr = (const char*)(wasm_memory->data);
    juce::String jsonString(json_cstr);
    
    std::cout << "JSON length: " << jsonString.length() << std::endl;
    
    // Parse JSON
    auto json = juce::JSON::parse(jsonString);
    if (!json.isObject())
    {
        std::cout << "ERROR: JSON is not an object!" << std::endl;
        return;
    }
    
    auto uiArray = json.getProperty("ui", juce::var()).getArray();
    if (uiArray == nullptr || uiArray->size() == 0)
    {
        std::cout << "ERROR: UI array not found or empty!" << std::endl;
        return;
    }
    
    std::cout << "Found UI array with " << uiArray->size() << " items" << std::endl;
    
    // Get the first vgroup (TS9_OverdriveFaustGenerated)
    auto vgroup = (*uiArray)[0];
    auto items = vgroup.getProperty("items", juce::var()).getArray();
    if (items == nullptr)
    {
        std::cout << "ERROR: Items array not found!" << std::endl;
        return;
    }
    
    std::cout << "Found " << items->size() << " top-level items" << std::endl;
    
    // Recursive function to process all parameters
    std::function<void(const juce::var&)> processUIItem = [&](const juce::var& item) {
        juce::String type = item.getProperty("type", "").toString();
        juce::String label = item.getProperty("label", "").toString();
        
        // If this is a group (hgroup/vgroup), process its items recursively
        if (type == "hgroup" || type == "vgroup")
        {
            std::cout << "Found group: " << label << std::endl;
            auto groupItems = item.getProperty("items", juce::var()).getArray();
            if (groupItems != nullptr)
            {
                for (auto& groupItem : *groupItems)
                {
                    processUIItem(groupItem);
                }
            }
            return;
        }
        
        // Process actual controls (sliders, checkboxes, etc.)
        int index = item.getProperty("index", -1);
        
        if (index == -1)
        {
            std::cout << "WARNING: Item " << label << " has no index!" << std::endl;
            return;
        }
        
        std::cout << "Processing param: " << label << " (type: " << type << ", index: " << index << ")" << std::endl;
        
        if (type == "hslider" || type == "vslider")
        {
            float minVal = item.getProperty("min", 0.0f);
            float maxVal = item.getProperty("max", 1.0f);
            float initVal = item.getProperty("init", 0.0f);
            
            // Override with custom defaults
            if (label == "drive") initVal = 0.48f;
            else if (label == "level") initVal = -8.0f;
            else if (label == "tone") initVal = 709.0f;
            
            std::cout << "  Range: " << minVal << " to " << maxVal << ", default: " << initVal << std::endl;
            
            parameterIndexMap[label] = index;
            
            // Add "TS9_" prefix to avoid conflicts with pitch shifter params
            juce::String paramID = "ts9_" + label;
            
            auto param = std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{paramID, 1},
                "TS9 " + label,
                minVal,
                maxVal,
                initVal
            );
            
            processor.addParameter(param.release());
            std::cout << "  Added float parameter: " << paramID << std::endl;
            
            // Set the default value in WASM
            w2c_ts9_setParamValue(&wasm_app, 0, index, initVal);
        }
        else if (type == "checkbox")
        {
            parameterIndexMap[label] = index;
            
            juce::String paramID = "ts9_" + label;
            
            auto param = std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{paramID, 1},
                "TS9 " + label,
                false
            );
            
            processor.addParameter(param.release());
            std::cout << "  Added bool parameter: " << paramID << std::endl;
            
            w2c_ts9_setParamValue(&wasm_app, 0, index, 0.0f);
        }
    };
    
    // Process all items recursively
    for (auto& item : *items)
    {
        processUIItem(item);
    }
    
    std::cout << "Initializing TS9 WASM with default parameters..." << std::endl;
    w2c_ts9_init(&wasm_app, 512, 48000);
    
    // Re-set all default values after init
    std::cout << "Re-setting default values after init..." << std::endl;
    for (auto* param : processor.getParameters())
    {
        juce::String paramName = param->getName(100);
        
        // Only process TS9 parameters
        if (!paramName.startsWith("TS9 "))
            continue;
            
        // Remove "TS9 " prefix to get original label
        juce::String originalLabel = paramName.substring(4);
        
        if (parameterIndexMap.count(originalLabel) > 0)
        {
            int wasmIndex = parameterIndexMap[originalLabel];
            float value = 0.0f;
            
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
            {
                juce::NormalisableRange<float> range = floatParam->getNormalisableRange();
                value = range.convertFrom0to1(param->getValue());
            }
            else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
            {
                value = boolParam->get() ? 1.0f : 0.0f;
            }
            
            w2c_ts9_setParamValue(&wasm_app, 0, wasmIndex, value);
            std::cout << "  [" << originalLabel << "] = " << value << std::endl;
        }
    }
    std::cout << "TS9 Initialization complete." << std::endl;
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
    std::cout << "=== prepareToPlay ===" << std::endl;
    std::cout << "Sample Rate: " << sampleRate << std::endl;
    std::cout << "Samples Per Block: " << samplesPerBlock << std::endl;
    
    // Re-initialize TS9 WASM with correct sample rate and buffer size
    std::cout << "Re-initializing TS9 WASM..." << std::endl;
    w2c_ts9_init(&ts9WasmApp, float(samplesPerBlock), float(sampleRate));
    
    // Restore TS9 parameter values after re-init
    std::cout << "Restoring TS9 parameter values..." << std::endl;
    for (auto* param : getParameters())
    {
        juce::String paramName = param->getName(100);
        
        // Only process TS9 parameters
        if (!paramName.startsWith("TS9 "))
            continue;
            
        // Remove "TS9 " prefix
        juce::String originalLabel = paramName.substring(4);
        
        if (ts9ParameterIndexMap.count(originalLabel) > 0)
        {
            int wasmIndex = ts9ParameterIndexMap[originalLabel];
            float value = 0.0f;
            
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
            {
                juce::NormalisableRange<float> range = floatParam->getNormalisableRange();
                value = range.convertFrom0to1(param->getValue());
            }
            else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
            {
                value = boolParam->get() ? 1.0f : 0.0f;
            }
            
            w2c_ts9_setParamValue(&ts9WasmApp, 0, wasmIndex, value);
        }
    }
    std::cout << "TS9 parameter restoration complete." << std::endl;
    
    // Initialize pitch shifters
    pitchShifterLeft.init(static_cast<int>(sampleRate));
    pitchShifterRight.init(static_cast<int>(sampleRate));

    // Create compressor with correct sample rate
    compressor = std::make_unique<giml::Compressor<float>>(static_cast<int>(sampleRate));
    compressor->enable();
    compressor->setParams(*compThreshParam, *compRatioParam, *compMakeupParam,
                          *compKneeParam, *compAttackParam, *compReleaseParam);
    
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

   #if ! JucePlugin_IsSynth
    // Allow mono input → stereo output (mono-to-stereo widening), as well as
    // matched mono→mono and stereo→stereo layouts.
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    const bool monoToStereo = (in == juce::AudioChannelSet::mono()
                             && out == juce::AudioChannelSet::stereo());
    if (!monoToStereo && in != out)
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

    // Check if we should use WAV file or real audio input
    bool useWavFile = useWavFileParam->get();
    
    if (useWavFile && audioFileBuffer.getNumSamples() > 0)
    {
        // ===== PROCESS WAV FILE =====
        int numSamples = buffer.getNumSamples();
        int numChannels = totalNumOutputChannels;
        int fileChannels = audioFileBuffer.getNumChannels();

        // ===== STEP 1: Read audio file data and prepare for TS9 processing =====
        // Create a mono buffer for TS9 WASM processing
        juce::AudioBuffer<float> ts9InputBuffer(1, numSamples);
        float* ts9InputData = ts9InputBuffer.getWritePointer(0);
        
        // Sum stereo file to mono for TS9 input
        for (int sample = 0; sample < numSamples; ++sample)
        {
            int pos = playbackPosition + sample;
            if (pos >= audioFileBuffer.getNumSamples())
                pos %= audioFileBuffer.getNumSamples(); // loop
            
            float sampleL = audioFileBuffer.getSample(0, pos);
            float sampleR = fileChannels > 1 ? audioFileBuffer.getSample(1, pos) : sampleL;
            ts9InputData[sample] = (sampleL + sampleR) * 0.5f; // Sum to mono
        }

        // Save uncompressed input as dry reference
        juce::AudioBuffer<float> rawInputBuffer(1, numSamples);
        float* rawInputData = rawInputBuffer.getWritePointer(0);
        rawInputBuffer.copyFrom(0, 0, ts9InputBuffer, 0, 0, numSamples);

        // ===== STEP 1b: Apply compressor (first in signal chain) =====
        if (compressor)
        {
            compressor->toggle(compEnabledParam->get());
            compressor->setParams(*compThreshParam, *compRatioParam, *compMakeupParam,
                                  *compKneeParam, *compAttackParam, *compReleaseParam);
            for (int i = 0; i < numSamples; ++i)
                ts9InputData[i] = compressor->processSample(ts9InputData[i]);
        }
        
        // ===== STEP 2: Process through TS9 WASM =====
        // Sync JUCE parameters to TS9 WASM
        for (auto* param : getParameters())
        {
            juce::String paramName = param->getName(100);
            
            if (!paramName.startsWith("TS9 "))
                continue;
                
            juce::String originalLabel = paramName.substring(4);
            
            if (ts9ParameterIndexMap.count(originalLabel) > 0)
            {
                int wasmIndex = ts9ParameterIndexMap[originalLabel];
                float value = 0.0f;
                
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
                {
                    juce::NormalisableRange<float> range = floatParam->getNormalisableRange();
                    value = range.convertFrom0to1(param->getValue());
                }
                else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
                {
                    value = boolParam->get() ? 1.0f : 0.0f;
                }
                
                w2c_ts9_setParamValue(&ts9WasmApp, 0, wasmIndex, value);
            }
        }
        
        // Setup TS9 WASM buffers in memory
        const u32 ts9_buffer_size = numSamples;
        const u32 ts9_input_buffer_offset = 1024;
        const u32 ts9_output_buffer_offset = 1024 + (ts9_buffer_size * sizeof(float));
        const u32 ts9_input_ptrs_offset = 1024 + (ts9_buffer_size * sizeof(float) * 2);
        const u32 ts9_output_ptrs_offset = ts9_input_ptrs_offset + sizeof(u32);
        
        // Write input to WASM memory
        float* ts9_wasm_input = (float*)(ts9WasmMemory->data + ts9_input_buffer_offset);
        for (u32 i = 0; i < ts9_buffer_size; i++)
        {
            ts9_wasm_input[i] = ts9InputData[i];
        }
        
        // Create input/output pointer arrays (mono processing)
        u32* ts9_input_ptrs = (u32*)(ts9WasmMemory->data + ts9_input_ptrs_offset);
        u32* ts9_output_ptrs = (u32*)(ts9WasmMemory->data + ts9_output_ptrs_offset);
        ts9_input_ptrs[0] = ts9_input_buffer_offset;
        ts9_output_ptrs[0] = ts9_output_buffer_offset;
        
        // Process through TS9
        w2c_ts9_compute(&ts9WasmApp, 0, ts9_buffer_size, ts9_input_ptrs_offset, ts9_output_ptrs_offset);
        
        // Read TS9 output from WASM memory
        float* ts9_wasm_output = (float*)(ts9WasmMemory->data + ts9_output_buffer_offset);
        juce::AudioBuffer<float> ts9OutputBuffer(1, numSamples);
        float* ts9OutputData = ts9OutputBuffer.getWritePointer(0);
        for (u32 i = 0; i < ts9_buffer_size; i++)
        {
            float sample = ts9_wasm_output[i];
            // Clamp to prevent explosions
            if (!std::isfinite(sample) || sample > 10.0f || sample < -10.0f)
                sample = 0.0f;
            ts9OutputData[i] = sample;
        }
        
        // ===== STEP 3: Apply pitch shifting =====
        const bool  fuzzEnabled = fuzzEnabledParam->get();
        const float downGain    = juce::Decibels::decibelsToGain(downShiftLevelParam->get());
        const float upGain      = juce::Decibels::decibelsToGain(upShiftLevelParam->get());

        // Update pitch shifter parameters once, before any processing
        pitchShifterLeft.fHslider1 = *leftShiftParam;    // shift (semitones)
        pitchShifterLeft.fHslider0 = *leftWindowParam;   // window (samples)
        pitchShifterLeft.fHslider2 = *leftXfadeParam;    // xfade (samples)
        pitchShifterRight.fHslider1 = *rightShiftParam;
        pitchShifterRight.fHslider0 = *rightWindowParam;
        pitchShifterRight.fHslider2 = *rightXfadeParam;

        // Compute downshift branch (compressed clean signal)
        juce::AudioBuffer<float> downShiftBuffer(1, numSamples);
        downShiftBuffer.copyFrom(0, 0, ts9InputBuffer, 0, 0, numSamples);
        {
            float* ptr[1] = { downShiftBuffer.getWritePointer(0) };
            pitchShifterLeft.compute(numSamples, ptr, ptr);
        }

        // Compute upshift branch (fuzzed or clean, depending on toggle)
        juce::AudioBuffer<float> upShiftBuffer(1, numSamples);
        {
            float* upData = upShiftBuffer.getWritePointer(0);
            const float* src = fuzzEnabled ? ts9OutputData : rawInputData;
            for (int i = 0; i < numSamples; ++i)
                upData[i] = src[i];
            float* ptr[1] = { upData };
            pitchShifterRight.compute(numSamples, ptr, ptr);
        }

        // Solo logic: when a branch is soloed, only that pitched signal is heard
        // (dry and the other branch are both muted)
        const bool soloDown  = downShiftSoloParam->get();
        const bool soloUp    = upShiftSoloParam->get();
        const bool anySolo   = soloDown || soloUp;
        const float dryScale       = anySolo ? 0.0f : 1.0f;
        const float activeDownGain = (!anySolo || soloDown) ? downGain : 0.0f;
        const float activeUpGain   = (!anySolo || soloUp)   ? upGain   : 0.0f;

        // Write to output channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* outputData        = buffer.getWritePointer(channel);
            const float* downData    = downShiftBuffer.getReadPointer(0);
            const float* upData      = upShiftBuffer.getReadPointer(0);

            if (numChannels == 1)
            {
                // Mono output: sum both shifted branches
                for (int sample = 0; sample < numSamples; ++sample)
                    outputData[sample] = rawInputData[sample] * dryScale
                                       + downData[sample] * activeDownGain
                                       + upData[sample]   * activeUpGain;
            }
            else
            {
                // Stereo output: ch0 = dry+down, ch1 = dry+up
                if (channel == 0)
                    for (int sample = 0; sample < numSamples; ++sample)
                        outputData[sample] = rawInputData[sample] * dryScale + downData[sample] * activeDownGain;
                else
                    for (int sample = 0; sample < numSamples; ++sample)
                        outputData[sample] = rawInputData[sample] * dryScale + upData[sample] * activeUpGain;
            }
        }

        // Advance playback position
        playbackPosition += numSamples;
        if (playbackPosition >= audioFileBuffer.getNumSamples())
            playbackPosition %= audioFileBuffer.getNumSamples();
    }
    else if (!useWavFile)
    {
        // ===== PROCESS REAL AUDIO INPUT =====
        int numSamples = buffer.getNumSamples();
        int numChannels = totalNumOutputChannels;
        
        // ===== STEP 1: Prepare real audio input for TS9 processing =====
        // Create a mono buffer for TS9 WASM processing from input buffer
        juce::AudioBuffer<float> ts9InputBuffer(1, numSamples);
        float* ts9InputData = ts9InputBuffer.getWritePointer(0);
        
        // Sum input channels to mono for TS9 input
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float sum = 0.0f;
            for (int channel = 0; channel < totalNumInputChannels; ++channel)
            {
                sum += buffer.getSample(channel, sample);
            }
            ts9InputData[sample] = sum / (float)totalNumInputChannels; // Average to mono
        }

        // Save uncompressed input as dry reference
        juce::AudioBuffer<float> rawInputBuffer(1, numSamples);
        float* rawInputData = rawInputBuffer.getWritePointer(0);
        rawInputBuffer.copyFrom(0, 0, ts9InputBuffer, 0, 0, numSamples);

        // ===== STEP 1b: Apply compressor (first in signal chain) =====
        if (compressor)
        {
            compressor->toggle(compEnabledParam->get());
            compressor->setParams(*compThreshParam, *compRatioParam, *compMakeupParam,
                                  *compKneeParam, *compAttackParam, *compReleaseParam);
            for (int i = 0; i < numSamples; ++i)
                ts9InputData[i] = compressor->processSample(ts9InputData[i]);
        }
        
        // ===== STEP 2: Process through TS9 WASM =====
        // Sync JUCE parameters to TS9 WASM
        for (auto* param : getParameters())
        {
            juce::String paramName = param->getName(100);
            
            if (!paramName.startsWith("TS9 "))
                continue;
                
            juce::String originalLabel = paramName.substring(4);
            
            if (ts9ParameterIndexMap.count(originalLabel) > 0)
            {
                int wasmIndex = ts9ParameterIndexMap[originalLabel];
                float value = 0.0f;
                
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
                {
                    juce::NormalisableRange<float> range = floatParam->getNormalisableRange();
                    value = range.convertFrom0to1(param->getValue());
                }
                else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
                {
                    value = boolParam->get() ? 1.0f : 0.0f;
                }
                
                w2c_ts9_setParamValue(&ts9WasmApp, 0, wasmIndex, value);
            }
        }
        
        // Setup TS9 WASM buffers in memory
        const u32 ts9_buffer_size = numSamples;
        const u32 ts9_input_buffer_offset = 1024;
        const u32 ts9_output_buffer_offset = 1024 + (ts9_buffer_size * sizeof(float));
        const u32 ts9_input_ptrs_offset = 1024 + (ts9_buffer_size * sizeof(float) * 2);
        const u32 ts9_output_ptrs_offset = ts9_input_ptrs_offset + sizeof(u32);
        
        // Write input to WASM memory
        float* ts9_wasm_input = (float*)(ts9WasmMemory->data + ts9_input_buffer_offset);
        for (u32 i = 0; i < ts9_buffer_size; i++)
        {
            ts9_wasm_input[i] = ts9InputData[i];
        }
        
        // Create input/output pointer arrays (mono processing)
        u32* ts9_input_ptrs = (u32*)(ts9WasmMemory->data + ts9_input_ptrs_offset);
        u32* ts9_output_ptrs = (u32*)(ts9WasmMemory->data + ts9_output_ptrs_offset);
        ts9_input_ptrs[0] = ts9_input_buffer_offset;
        ts9_output_ptrs[0] = ts9_output_buffer_offset;
        
        // Process through TS9
        w2c_ts9_compute(&ts9WasmApp, 0, ts9_buffer_size, ts9_input_ptrs_offset, ts9_output_ptrs_offset);
        
        // Read TS9 output from WASM memory
        float* ts9_wasm_output = (float*)(ts9WasmMemory->data + ts9_output_buffer_offset);
        juce::AudioBuffer<float> ts9OutputBuffer(1, numSamples);
        float* ts9OutputData = ts9OutputBuffer.getWritePointer(0);
        for (u32 i = 0; i < ts9_buffer_size; i++)
        {
            float sample = ts9_wasm_output[i];
            // Clamp to prevent explosions
            if (!std::isfinite(sample) || sample > 10.0f || sample < -10.0f)
                sample = 0.0f;
            ts9OutputData[i] = sample;
        }
        
        // ===== STEP 3: Apply pitch shifting =====
        const bool  fuzzEnabled = fuzzEnabledParam->get();
        const float downGain    = juce::Decibels::decibelsToGain(downShiftLevelParam->get());
        const float upGain      = juce::Decibels::decibelsToGain(upShiftLevelParam->get());

        // Update pitch shifter parameters once, before any processing
        pitchShifterLeft.fHslider1 = *leftShiftParam;
        pitchShifterLeft.fHslider0 = *leftWindowParam;
        pitchShifterLeft.fHslider2 = *leftXfadeParam;
        pitchShifterRight.fHslider1 = *rightShiftParam;
        pitchShifterRight.fHslider0 = *rightWindowParam;
        pitchShifterRight.fHslider2 = *rightXfadeParam;

        // Compute downshift branch (compressed clean signal)
        juce::AudioBuffer<float> downShiftBuffer(1, numSamples);
        downShiftBuffer.copyFrom(0, 0, ts9InputBuffer, 0, 0, numSamples);
        {
            float* ptr[1] = { downShiftBuffer.getWritePointer(0) };
            pitchShifterLeft.compute(numSamples, ptr, ptr);
        }

        // Compute upshift branch (fuzzed or clean, depending on toggle)
        juce::AudioBuffer<float> upShiftBuffer(1, numSamples);
        {
            float* upData = upShiftBuffer.getWritePointer(0);
            const float* src = fuzzEnabled ? ts9OutputData : rawInputData;
            for (int i = 0; i < numSamples; ++i)
                upData[i] = src[i];
            float* ptr[1] = { upData };
            pitchShifterRight.compute(numSamples, ptr, ptr);
        }

        // Solo logic: when a branch is soloed, only that pitched signal is heard
        // (dry and the other branch are both muted)
        const bool soloDown  = downShiftSoloParam->get();
        const bool soloUp    = upShiftSoloParam->get();
        const bool anySolo   = soloDown || soloUp;
        const float dryScale       = anySolo ? 0.0f : 1.0f;
        const float activeDownGain = (!anySolo || soloDown) ? downGain : 0.0f;
        const float activeUpGain   = (!anySolo || soloUp)   ? upGain   : 0.0f;

        // Write to output channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* outputData        = buffer.getWritePointer(channel);
            const float* downData    = downShiftBuffer.getReadPointer(0);
            const float* upData      = upShiftBuffer.getReadPointer(0);

            if (numChannels == 1)
            {
                // Mono output: sum both shifted branches
                for (int sample = 0; sample < numSamples; ++sample)
                    outputData[sample] = rawInputData[sample] * dryScale
                                       + downData[sample] * activeDownGain
                                       + upData[sample]   * activeUpGain;
            }
            else
            {
                // Stereo output: ch0 = dry+down, ch1 = dry+up
                if (channel == 0)
                    for (int sample = 0; sample < numSamples; ++sample)
                        outputData[sample] = rawInputData[sample] * dryScale + downData[sample] * activeDownGain;
                else
                    for (int sample = 0; sample < numSamples; ++sample)
                        outputData[sample] = rawInputData[sample] * dryScale + upData[sample] * activeUpGain;
            }
        }
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
