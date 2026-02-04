#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : GenericAudioProcessorEditor (p), processorRef (p)
{
    // The GenericAudioProcessorEditor will automatically create controls for all parameters
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}
