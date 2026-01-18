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
    createParametersAndInitWasm(*this, wasm_app, wasm_memory, parameterIndexMap);
    
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
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

void AudioPluginAudioProcessor::createParametersAndInitWasm(juce::AudioProcessor& processor,
                                                           w2c_muff& wasm_app,
                                                           wasm_rt_memory_t*& wasm_memory,
                                                           std::map<juce::String, int>& parameterIndexMap)
{
    std::cout << "Creating parameters and initializing WASM..." << std::endl;
    
    // Initialize WASM runtime and module
    wasm_rt_init();
    wasm2c_muff_instantiate(&wasm_app, wasm_app.w2c_env_instance);
    wasm_memory = w2c_muff_memory(&wasm_app);
    
    std::cout << "WASM memory pointer: " << (void*)wasm_memory << std::endl;
    std::cout << "WASM memory data: " << (void*)wasm_memory->data << std::endl;
    std::cout << "WASM memory size: " << wasm_memory->size << std::endl;
    
    // Check first few bytes
    std::cout << "First 20 bytes: ";
    for (int i = 0; i < 20 && i < wasm_memory->size; i++) {
        std::cout << (int)(unsigned char)wasm_memory->data[i] << " ";
    }
    std::cout << std::endl;
    
    // Read JSON metadata from WASM memory BEFORE calling init (which overwrites it)
    const char* json_cstr = (const char*)(wasm_memory->data);
    juce::String jsonString(json_cstr);
    
    std::cout << "JSON length: " << jsonString.length() << std::endl;
    std::cout << "JSON first 100 chars: " << jsonString.substring(0, 100) << std::endl;
    
    // Parse JSON
    auto json = juce::JSON::parse(jsonString);
    if (!json.isObject())
    {
        std::cout << "ERROR: JSON is not an object!" << std::endl;
        return;
    }
    
    std::cout << "JSON parsed successfully" << std::endl;
    
    auto uiArray = json.getProperty("ui", juce::var()).getArray();
    if (uiArray == nullptr || uiArray->size() == 0)
    {
        std::cout << "ERROR: UI array not found or empty!" << std::endl;
        return;
    }
    
    std::cout << "Found UI array with " << uiArray->size() << " items" << std::endl;
    
    // Get the BigMuff vgroup
    auto vgroup = (*uiArray)[0];
    auto items = vgroup.getProperty("items", juce::var()).getArray();
    if (items == nullptr)
    {
        std::cout << "ERROR: Items array not found!" << std::endl;
        return;
    }
    
    std::cout << "Found " << items->size() << " parameter items" << std::endl;
    
    // Create JUCE parameters from metadata
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    for (auto& item : *items)
    {
        juce::String type = item.getProperty("type", "").toString();
        juce::String label = item.getProperty("label", "").toString();
        int index = item.getProperty("index", 0);
        
        std::cout << "Processing param: " << label << " (type: " << type << ", index: " << index << ")" << std::endl;
        
        if (type == "hslider" || type == "vslider")
        {
            float minVal = item.getProperty("min", 0.0f);
            float maxVal = item.getProperty("max", 1.0f);
            float initVal = item.getProperty("init", 0.0f);
            
            std::cout << "  Range: " << minVal << " to " << maxVal << ", default: " << initVal << std::endl;
            
            parameterIndexMap[label] = index;
            
            auto param = std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{label, 1},
                label,
                minVal,
                maxVal,
                initVal
            );
            
            processor.addParameter(param.release());
            std::cout << "  Added float parameter" << std::endl;
            
            // Set the default value in WASM
            w2c_muff_setParamValue(&wasm_app, 0, index, initVal);
            float readback = w2c_muff_getParamValue(&wasm_app, 0, index);
            std::cout << "  Set WASM default value: " << initVal << " (readback: " << readback << ")" << std::endl;
        }
        else if (type == "checkbox")
        {
            parameterIndexMap[label] = index;
            
            auto param = std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID{label, 1},
                label,
                false
            );
            
            processor.addParameter(param.release());
            std::cout << "  Added bool parameter" << std::endl;
            
            // Set the default value in WASM (false = 0.0)
            w2c_muff_setParamValue(&wasm_app, 0, index, 0.0f);
            float readback = w2c_muff_getParamValue(&wasm_app, 0, index);
            std::cout << "  Set WASM default value: 0.0 (readback: " << readback << ")" << std::endl;
        }
    }
    
    std::cout << "Finished creating " << processor.getParameters().size() << " parameters" << std::endl;
    
    // Now initialize WASM with default sample rate (this will be re-initialized in prepareToPlay with actual values)
    std::cout << "Initializing WASM with default parameters..." << std::endl;
    w2c_muff_init(&wasm_app, 512, 48000);
    
    // Re-set all default values after init
    std::cout << "Re-setting default values after init..." << std::endl;
    for (auto* param : processor.getParameters())
    {
        juce::String paramName = param->getName(100);
        if (parameterIndexMap.count(paramName) > 0)
        {
            int wasmIndex = parameterIndexMap[paramName];
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
            
            w2c_muff_setParamValue(&wasm_app, 0, wasmIndex, value);
            std::cout << "  [" << paramName << "] = " << value << std::endl;
        }
    }
    std::cout << "Initialization complete." << std::endl;
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
    std::cout << "+++++ INIT CALLED +++++" << std::endl;
    std::cout << "Sample Rate: " << sampleRate << std::endl;
    std::cout << "Samples Per Block: " << samplesPerBlock << std::endl;

    // Re-initialize WASM with correct sample rate and buffer size
    std::cout << "Re-initializing WASM with correct parameters..." << std::endl;
    w2c_muff_init(&wasm_app, float(samplesPerBlock), float(sampleRate));
    std::cout << "WASM module initialized successfully!" << std::endl;
    
    // Restore parameter values after re-init
    std::cout << "Restoring parameter values..." << std::endl;
    for (auto* param : getParameters())
    {
        juce::String paramName = param->getName(100);
        if (parameterIndexMap.count(paramName) > 0)
        {
            int wasmIndex = parameterIndexMap[paramName];
            float value = 0.0f;
            
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
            {
                juce::NormalisableRange<float> range = floatParam->getNormalisableRange();
                value = range.convertFrom0to1(param->getValue());
                std::cout << "  [" << paramName << "] JUCE norm: " << param->getValue() 
                          << ", scaled: " << value 
                          << ", range: [" << range.start << ", " << range.end << "]" << std::endl;
            }
            else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
            {
                value = boolParam->get() ? 1.0f : 0.0f;
                std::cout << "  [" << paramName << "] bool: " << value << std::endl;
            }
            
            w2c_muff_setParamValue(&wasm_app, 0, wasmIndex, value);
            
            // Verify it was set
            float readback = w2c_muff_getParamValue(&wasm_app, 0, wasmIndex);
            std::cout << "    WASM readback: " << readback << std::endl;
        }
    }
    std::cout << "Parameter restoration complete." << std::endl;

    std::cout << "Getting WASM module sample rate..." << std::endl;
    u32 wasm_sample_rate = w2c_muff_getSampleRate(&wasm_app, 0);
    std::cout << "WASM Sample Rate: " << wasm_sample_rate << std::endl;
    std::cout << "JUCE Sample Rate: " << sampleRate << std::endl;

    // Test parameters
    // std::cout << std::endl << "=== PARAMETER TESTING ===" << std::endl;
    
    // // Get default parameter values
    // std::cout << "Default parameter values:" << std::endl;
    // float bypass_default = w2c_muff_getParamValue(&wasm_app, 0, 12);
    // float output_default = w2c_muff_getParamValue(&wasm_app, 0, 52);
    // float drive_default = w2c_muff_getParamValue(&wasm_app, 0, 56);
    // float tone_default = w2c_muff_getParamValue(&wasm_app, 0, 60);
    // float input_default = w2c_muff_getParamValue(&wasm_app, 0, 80);
    
    // std::cout << "  bypass (index 12): " << bypass_default << std::endl;
    // std::cout << "  output (index 52): " << output_default << std::endl;
    // std::cout << "  drive  (index 56): " << drive_default << std::endl;
    // std::cout << "  tone   (index 60): " << tone_default << std::endl;
    // std::cout << "  input  (index 80): " << input_default << std::endl;
    
    // Set new parameter values
    // std::cout << std::endl << "Setting new parameter values..." << std::endl;
    // w2c_muff_setParamValue(&wasm_app, 0, 12, 0.0f);    // bypass off
    // w2c_muff_setParamValue(&wasm_app, 0, 52, 85.0f);   // output = 85%
    // w2c_muff_setParamValue(&wasm_app, 0, 56, 45.0f);   // drive = 45
    // w2c_muff_setParamValue(&wasm_app, 0, 60, 0.75f);   // tone = 0.75
    // w2c_muff_setParamValue(&wasm_app, 0, 80, 6.0f);    // input = 6 dB
    // std::cout << "  bypass = 0.0" << std::endl;
    // std::cout << "  output = 85.0" << std::endl;
    // std::cout << "  drive  = 45.0" << std::endl;
    // std::cout << "  tone   = 0.75" << std::endl;
    // std::cout << "  input  = 6.0" << std::endl;
    
    // Verify the new values
    // std::cout << std::endl << "Verifying new parameter values:" << std::endl;
    // float bypass_verify = w2c_muff_getParamValue(&wasm_app, 0, 12);
    // float output_verify = w2c_muff_getParamValue(&wasm_app, 0, 52);
    // float drive_verify = w2c_muff_getParamValue(&wasm_app, 0, 56);
    // float tone_verify = w2c_muff_getParamValue(&wasm_app, 0, 60);
    // float input_verify = w2c_muff_getParamValue(&wasm_app, 0, 80);
    
    // std::cout << "  bypass (index 12): " << bypass_verify << std::endl;
    // std::cout << "  output (index 52): " << output_verify << std::endl;
    // std::cout << "  drive  (index 56): " << drive_verify << std::endl;
    // std::cout << "  tone   (index 60): " << tone_verify << std::endl;
    // std::cout << "  input  (index 80): " << input_verify << std::endl;
    // std::cout << "=== PARAMETER TEST COMPLETE ===" << std::endl << std::endl;

    std::cout << "Testing WASM compute function..." << std::endl;
    
    // Get access to WASM memory
    wasm_memory = w2c_muff_memory(&wasm_app);
    std::cout << "WASM memory size: " << wasm_memory->size << " bytes" << std::endl;
    
    // Allocate buffers in WASM memory (we'll use the end of memory for our test buffers)
    const u32 test_buffer_size = samplesPerBlock; // Process one block
    const u32 input_buffer_offset = 1024;  // Arbitrary safe offset in WASM memory
    const u32 output_buffer_offset = 1024 + (test_buffer_size * sizeof(float));
    const u32 input_ptrs_offset = output_buffer_offset + (test_buffer_size * sizeof(float));
    const u32 output_ptrs_offset = input_ptrs_offset + sizeof(u32);
    
    // Write test input data to WASM memory
    float* input_buffer = (float*)(wasm_memory->data + input_buffer_offset);
    for (u32 i = 0; i < test_buffer_size; i++) {
        input_buffer[i] = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
    }
    
    // Create input/output pointer arrays (float** pattern)
    u32* input_ptrs = (u32*)(wasm_memory->data + input_ptrs_offset);
    u32* output_ptrs = (u32*)(wasm_memory->data + output_ptrs_offset);
    input_ptrs[0] = input_buffer_offset;   // Pointer to input channel 0
    output_ptrs[0] = output_buffer_offset; // Pointer to output channel 0
    
    std::cout << "Processing " << test_buffer_size << " samples..." << std::endl;
    w2c_muff_compute(&wasm_app, 0, test_buffer_size, input_ptrs_offset, output_ptrs_offset);
    
    // Read results from WASM memory
    float* output_buffer = (float*)(wasm_memory->data + output_buffer_offset);
    std::cout << "First 8 samples:" << std::endl;
    for (u32 i = 0; i < 8 && i < test_buffer_size; i++) {
        std::cout << std::fixed << std::setprecision(3) 
                  << "  [" << i << "] in=" << input_buffer[i] 
                  << " out=" << output_buffer[i] << std::endl;
    }
    
    // juce::JUCEApplication::quit();

    // // BENCHMARKING BELOW

    // // Benchmark configuration
    // const int WARMUP_RUNS = 10;
    // const int BENCHMARK_RUNS = 48000;  // 1 second at 48kHz

    // // Warmup phase to stabilize caches and branch prediction
    // std::cout << std::endl;
    // std::cout << "[WARMUP] Running " << WARMUP_RUNS << " warmup iterations..." << std::endl;
    // volatile float warmup_result = 0.0f;  // Prevent optimization
    // for (int i = 0; i < WARMUP_RUNS; i++) {
    //     float a = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
    //     warmup_result += w2c_muff_process(&wasm_app, a);
    // }
    // std::cout << "[OK] Warmup complete (result=" << std::fixed << std::setprecision(3) << warmup_result << ")" << std::endl;

    // // Benchmark phase with random inputs
    // std::cout << std::endl;
    // std::cout << "[BENCHMARK] Running " << BENCHMARK_RUNS << " iterations..." << std::endl;

    // std::vector<double> timings;
    // volatile float checksum = 0.0f;  // Prevent optimization

    // for (int i = 0; i < BENCHMARK_RUNS; i++) {
    //     // Use random audio samples in range -1.0 to 1.0
    //     float a = (float)(juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);

    //     auto start = std::chrono::high_resolution_clock::now();
    //     float result = w2c_muff_process(&wasm_app, a);
    //     auto end = std::chrono::high_resolution_clock::now();

    //     double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
    //     timings.push_back(elapsed_us);

    //     // Use result to prevent dead code elimination
    //     // checksum += result;
    // }
    // double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    // double avg_us = sum / timings.size();
    // std::sort(timings.begin(), timings.end());
    // double median_us = timings[timings.size() / 2];
    // double min_us = timings[0];
    // double max_us = timings[timings.size() - 1];
    // double variance = 0.0;
    // for (double t : timings) {
    //     variance += (t - avg_us) * (t - avg_us);
    // }
    // variance /= timings.size();
    // double stddev_us = std::sqrt(variance);
    
    // std::cout << std::endl;
    // std::cout << "=== BENCHMARK RESULTS ===" << std::endl;
    // std::cout << "Iterations: " << BENCHMARK_RUNS << std::endl;
    // std::cout << "Average:    " << std::fixed << std::setprecision(3) << avg_us << " us" << std::endl;
    // std::cout << "Median:     " << std::fixed << std::setprecision(3) << median_us << " us" << std::endl;
    // std::cout << "Std Dev:    " << std::fixed << std::setprecision(3) << stddev_us << " us" << std::endl;
    // std::cout << "Minimum:    " << std::fixed << std::setprecision(3) << min_us << " us" << std::endl;
    // std::cout << "Maximum:    " << std::fixed << std::setprecision(3) << max_us << " us" << std::endl;
    // std::cout << "Checksum:   " << std::fixed << std::setprecision(3) << checksum << " (prevents optimization)" << std::endl;
    
    // // Verify correctness with known values
    // float verify_in = 0.5f;
    // float verify_out = w2c_muff_process(&wasm_app, verify_in);
    // std::cout << std::endl;
    // std::cout << "Verification: input=" << std::fixed << std::setprecision(3) << verify_in << ", output=" << std::fixed << std::setprecision(3) << verify_out << std::endl;
    // std::cout << "[SUCCESS] WASM2C benchmark complete!" << std::endl;


    // std::cout << "+++++ TEST COMPLETE +++++" << std::endl;

    // juce::JUCEApplication::quit();

    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
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

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    // for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    // {
    //     float input = buffer.getReadPointer(0)[sample];
    //     float output = std::sinf(w2c_app_process(&wasm_app, input) * M_PI * 2.0f);
    //     buffer.getWritePointer(0)[sample] = output * 0.2f;
    // }


    
    // Allocate buffers in WASM memory (we'll use the end of memory for our test buffers)
    const u32 test_buffer_size = buffer.getNumSamples(); // Process one block
    const u32 input_buffer_offset = 1024;  // Arbitrary safe offset in WASM memory
    const u32 output_buffer_offset = 1024 + (test_buffer_size * sizeof(float));
    const u32 input_ptrs_offset = output_buffer_offset + (test_buffer_size * sizeof(float));
    const u32 output_ptrs_offset = input_ptrs_offset + sizeof(u32);
    
    // Write audio file data to WASM memory (looped playback instead of microphone input)
    float* input_buffer = (float*)(wasm_memory->data + input_buffer_offset);
    
    if (audioFileBuffer.getNumSamples() > 0)
    {
        // Use the audio file as input (looped)
        for (u32 i = 0; i < test_buffer_size; i++)
        {
            // Get sample from audio file buffer with looping
            int filePosition = (playbackPosition + i) % audioFileBuffer.getNumSamples();
            
            // Use first channel of the audio file
            input_buffer[i] = audioFileBuffer.getSample(0, filePosition);
        }
        
        // Update playback position for next block
        playbackPosition = (playbackPosition + test_buffer_size) % audioFileBuffer.getNumSamples();
    }
    else
    {
        // Fallback: use the original input from the buffer (microphone)
        for (u32 i = 0; i < test_buffer_size; i++)
        {
            input_buffer[i] = buffer.getReadPointer(0)[i];
        }
    }
    
    // Create input/output pointer arrays (float** pattern)
    u32* input_ptrs = (u32*)(wasm_memory->data + input_ptrs_offset);
    u32* output_ptrs = (u32*)(wasm_memory->data + output_ptrs_offset);
    input_ptrs[0] = input_buffer_offset;   // Pointer to input channel 0
    output_ptrs[0] = output_buffer_offset; // Pointer to output channel 0
    
    // Sync JUCE parameters to WASM before processing
    for (auto* param : getParameters())
    {
        juce::String paramName = param->getName(100);
        if (parameterIndexMap.count(paramName) > 0)
        {
            int wasmIndex = parameterIndexMap[paramName];
            float value = 0.0f;
            
            // For AudioParameterFloat, scale by range
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
            {
                juce::NormalisableRange<float> range = floatParam->getNormalisableRange();
                value = range.convertFrom0to1(param->getValue());
            }
            else if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
            {
                value = boolParam->get() ? 1.0f : 0.0f;
            }
            
            w2c_muff_setParamValue(&wasm_app, 0, wasmIndex, value);
        }
    }
    
    // std::cout << "Processing " << test_buffer_size << " samples..." << std::endl;
    w2c_muff_compute(&wasm_app, 0, test_buffer_size, input_ptrs_offset, output_ptrs_offset);
    
    // Read results from WASM memory
    float* output_buffer = (float*)(wasm_memory->data + output_buffer_offset);
    for (u32 i = 0; i < test_buffer_size; i++) {
        float sample = output_buffer[i];
        // Clamp to prevent explosions (safety measure)
        if (!std::isfinite(sample) || sample > 10.0f || sample < -10.0f) {
            sample = 0.0f;
        }
        buffer.getWritePointer(0)[i] = sample;
    }

    // copy channel 0 to all channels
    for (int channel = 1; channel < totalNumOutputChannels; ++channel)
    {
        buffer.copyFrom(channel, 0, buffer, 0, 0, buffer.getNumSamples());
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
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
