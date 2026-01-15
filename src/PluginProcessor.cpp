#include "PluginProcessor.h"
#include "PluginEditor.h"
// #include "wasm-app.h"

#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
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
    std::cout << "+++++ INIT CALLED +++++" << std::endl;
    std::cout << "Sample Rate: " << sampleRate << std::endl;
    std::cout << "Samples Per Block: " << samplesPerBlock << std::endl;

    std::cout << "Initializing WebAssembly..." << std::endl;
    wasm_rt_init();
    std::cout << "WASM runtime initialized." << std::endl;

    // w2c_app wasm_app;
    std::cout << "Instantiating WASM module..." << std::endl;
    wasm2c_app_instantiate(&wasm_app); // 
    std::cout << "WASM initialized successfully!" << std::endl;

    std::cout << "Testing WASM process function..." << std::endl;
    float test_in = 0.5f;
    float test_out = w2c_app_process(&wasm_app, test_in);
    std::cout << std::fixed << std::setprecision(3) << "Test: input=" << test_in << ", output=" << test_out << std::endl;

    // BENCHMARKING BELOW

    // Benchmark configuration
    const int WARMUP_RUNS = 10;
    const int BENCHMARK_RUNS = 48000;  // 1 second at 48kHz

    // Warmup phase to stabilize caches and branch prediction
    std::cout << std::endl;
    std::cout << "[WARMUP] Running " << WARMUP_RUNS << " warmup iterations..." << std::endl;
    volatile float warmup_result = 0.0f;  // Prevent optimization
    for (int i = 0; i < WARMUP_RUNS; i++) {
        float a = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        warmup_result += w2c_app_process(&wasm_app, a);
    }
    std::cout << "[OK] Warmup complete (result=" << std::fixed << std::setprecision(3) << warmup_result << ")" << std::endl;

    // Benchmark phase with random inputs
    std::cout << std::endl;
    std::cout << "[BENCHMARK] Running " << BENCHMARK_RUNS << " iterations..." << std::endl;

    std::vector<double> timings;
    volatile float checksum = 0.0f;  // Prevent optimization

    for (int i = 0; i < BENCHMARK_RUNS; i++) {
        // Use random audio samples in range -1.0 to 1.0
        float a = (float)(juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);

        auto start = std::chrono::high_resolution_clock::now();
        float result = w2c_app_process(&wasm_app, a);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        timings.push_back(elapsed_us);

        // Use result to prevent dead code elimination
        checksum += result;
    }
    double sum = std::accumulate(timings.begin(), timings.end(), 0.0);
    double avg_us = sum / timings.size();
    std::sort(timings.begin(), timings.end());
    double median_us = timings[timings.size() / 2];
    double min_us = timings[0];
    double max_us = timings[timings.size() - 1];
    double variance = 0.0;
    for (double t : timings) {
        variance += (t - avg_us) * (t - avg_us);
    }
    variance /= timings.size();
    double stddev_us = std::sqrt(variance);
    
    std::cout << std::endl;
    std::cout << "=== BENCHMARK RESULTS ===" << std::endl;
    std::cout << "Iterations: " << BENCHMARK_RUNS << std::endl;
    std::cout << "Average:    " << std::fixed << std::setprecision(3) << avg_us << " us" << std::endl;
    std::cout << "Median:     " << std::fixed << std::setprecision(3) << median_us << " us" << std::endl;
    std::cout << "Std Dev:    " << std::fixed << std::setprecision(3) << stddev_us << " us" << std::endl;
    std::cout << "Minimum:    " << std::fixed << std::setprecision(3) << min_us << " us" << std::endl;
    std::cout << "Maximum:    " << std::fixed << std::setprecision(3) << max_us << " us" << std::endl;
    std::cout << "Checksum:   " << std::fixed << std::setprecision(3) << checksum << " (prevents optimization)" << std::endl;
    
    // Verify correctness with known values
    float verify_in = 0.5f;
    float verify_out = w2c_app_process(&wasm_app, verify_in);
    std::cout << std::endl;
    std::cout << "Verification: input=" << std::fixed << std::setprecision(3) << verify_in << ", output=" << std::fixed << std::setprecision(3) << verify_out << std::endl;
    std::cout << "[SUCCESS] WASM2C benchmark complete!" << std::endl;


    std::cout << "+++++ TEST COMPLETE +++++" << std::endl;

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
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float input = buffer.getReadPointer(0)[sample];
        float output = std::sinf(w2c_app_process(&wasm_app, input) * M_PI * 2.0f);
        buffer.getWritePointer(0)[sample] = output * 0.2f;
    }

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
