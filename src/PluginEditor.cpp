#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Configure bypass button
    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);
    bypassButton.onClick = [this]() {
        auto* param = processorRef.getParameters()[0];
        if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(param))
        {
            *boolParam = bypassButton.getToggleState();
        }
    };
    addAndMakeVisible(bypassButton);
    
    // Configure parameter controls
    auto& params = processorRef.getParameters();
    int sliderIndex = 0;
    
    for (int i = 1; i < params.size() && sliderIndex < 10; ++i)
    {
        auto* param = params[i];
        if (dynamic_cast<juce::AudioParameterFloat*>(param))
        {
            parameterSliders[sliderIndex].setSliderStyle(juce::Slider::RotaryVerticalDrag);
            parameterSliders[sliderIndex].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
            parameterSliders[sliderIndex].setRange(0.0, 1.0, 0.01);
            parameterSliders[sliderIndex].setValue(param->getValue());
            parameterSliders[sliderIndex].onValueChange = [this, param, idx = sliderIndex]() {
                param->setValueNotifyingHost(parameterSliders[idx].getValue());
            };
            addAndMakeVisible(parameterSliders[sliderIndex]);
            
            parameterLabels[sliderIndex].setText(param->getName(32), juce::dontSendNotification);
            parameterLabels[sliderIndex].setJustificationType(juce::Justification::centred);
            addAndMakeVisible(parameterLabels[sliderIndex]);
            
            sliderIndex++;
        }
    }
    
    setSize (600, 400);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour(0xff1a1a1a));
    
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawFittedText ("Spring Reverb", 10, 10, getWidth() - 20, 40, juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(60);
    
    // Bypass button at top
    auto bypassArea = bounds.removeFromTop(60).reduced(20);
    bypassButton.setBounds(bypassArea.withSizeKeepingCentre(120, 40));
    
    bounds.removeFromTop(20);
    
    // Parameter knobs in a row
    auto knobArea = bounds.reduced(20);
    int numVisibleSliders = 0;
    for (int i = 0; i < 10; ++i)
    {
        if (parameterSliders[i].isVisible())
            numVisibleSliders++;
    }
    
    if (numVisibleSliders > 0)
    {
        int knobWidth = juce::jmin(100, knobArea.getWidth() / numVisibleSliders - 10);
        int x = (knobArea.getWidth() - (knobWidth + 10) * numVisibleSliders) / 2;
        
        for (int i = 0; i < 10; ++i)
        {
            if (parameterSliders[i].isVisible())
            {
                auto knobBounds = juce::Rectangle<int>(x, knobArea.getY(), knobWidth, knobWidth + 40);
                parameterSliders[i].setBounds(knobBounds.removeFromTop(knobWidth));
                parameterLabels[i].setBounds(knobBounds);
                x += knobWidth + 10;
            }
        }
    }
}
