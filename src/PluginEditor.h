#pragma once

#include "PluginProcessor.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& processorRef;

    void setupRotary (juce::Slider& s, juce::Label& l,
                      const juce::String& name, juce::Colour accent);
    void setupToggle (juce::ToggleButton& b, const juce::String& name);
    void layoutKnobRow (juce::Rectangle<int> area,
                        std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs);

    // ── Global toggles ─────────────────────────────────────────────────────
    juce::Label       titleLabel;
    juce::ToggleButton wavToggle  { "Test File Playback" };
    juce::ToggleButton fuzzToggle { "Fuzz"     };
    std::unique_ptr<juce::ButtonParameterAttachment> wavAttach, fuzzAttach;

    // ── Compressor ──────────────────────────────────────────────────────────
    juce::Label        compSectionLabel;
    juce::ToggleButton compEnabledToggle { "On" };
    std::unique_ptr<juce::ButtonParameterAttachment> compEnabledAttach;

    juce::Slider compThreshSlider,  compRatioSlider,   compKneeSlider;
    juce::Slider compAttackSlider,  compReleaseSlider, compMakeupSlider;
    juce::Label  compThreshLabel,   compRatioLabel,    compKneeLabel;
    juce::Label  compAttackLabel,   compReleaseLabel,  compMakeupLabel;
    std::unique_ptr<juce::SliderParameterAttachment>
        compThreshAttach, compRatioAttach,   compKneeAttach,
        compAttackAttach, compReleaseAttach, compMakeupAttach;

    // ── Fuzz / TS9 ──────────────────────────────────────────────────────────
    juce::Label  fuzzSectionLabel;
    juce::Slider ts9DriveSlider, ts9LevelSlider, ts9ToneSlider;
    juce::Label  ts9DriveLabel,  ts9LevelLabel,  ts9ToneLabel;
    std::unique_ptr<juce::SliderParameterAttachment>
        ts9DriveAttach, ts9LevelAttach, ts9ToneAttach;

    // ── Mix ─────────────────────────────────────────────────────────────────
    juce::Label  mixSectionLabel;
    juce::Slider downShiftSlider, upShiftSlider;
    juce::Label  downShiftLabel,  upShiftLabel;
    std::unique_ptr<juce::SliderParameterAttachment>
        downShiftAttach, upShiftAttach;

    juce::TextButton downSoloButton { "Solo" };
    juce::TextButton upSoloButton   { "Solo" };
    std::unique_ptr<juce::ButtonParameterAttachment>
        downSoloAttach, upSoloAttach;

    // Cached panel bounds for paint()
    juce::Rectangle<int> compPanelBounds, fuzzPanelBounds, mixPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
