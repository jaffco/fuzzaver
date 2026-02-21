#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace {
    const juce::Colour kBg    { 0xff1a1a2e };
    const juce::Colour kPanel { 0xff16213e };
    const juce::Colour kText  { 0xffe0e0e0 };
    const juce::Colour kSub   { 0xff7a7a8a };
    const juce::Colour kComp  { 0xff2a6496 }; // blue
    const juce::Colour kFuzz  { 0xff7a35a0 }; // purple
    const juce::Colour kMix   { 0xff1a934a }; // green
}

// Find a RangedAudioParameter by its ParameterID string
static juce::RangedAudioParameter* findByID (juce::AudioProcessor& proc, const juce::String& id)
{
    for (auto* p : proc.getParameters())
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
            if (wp->paramID == id)
                return dynamic_cast<juce::RangedAudioParameter*>(p);
    return nullptr;
}

//==============================================================================
void AudioPluginAudioProcessorEditor::setupRotary (juce::Slider& s, juce::Label& l,
                                                    const juce::String& name,
                                                    juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,    accent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kPanel.brighter (0.3f));
    s.setColour (juce::Slider::textBoxTextColourId,         kSub);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    s.setColour (juce::Slider::thumbColourId,               kText);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setFont (juce::Font (11.0f));
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, kSub);
    addAndMakeVisible (l);
}

void AudioPluginAudioProcessorEditor::setupToggle (juce::ToggleButton& b, const juce::String& name)
{
    b.setButtonText (name);
    b.setColour (juce::ToggleButton::textColourId,        kText);
    b.setColour (juce::ToggleButton::tickColourId,        kMix);
    b.setColour (juce::ToggleButton::tickDisabledColourId, kSub);
    addAndMakeVisible (b);
}

void AudioPluginAudioProcessorEditor::layoutKnobRow (
    juce::Rectangle<int> area,
    std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs)
{
    const int n = (int) knobs.size();
    if (n == 0) return;
    const int slotW  = area.getWidth() / n;
    const int labelH = 18;
    int x = area.getX();

    for (auto& [slider, label] : knobs)
    {
        auto slot = area.withX (x).withWidth (slotW);
        x += slotW;
        label->setBounds  (slot.removeFromBottom (labelH));
        slider->setBounds (slot);
    }
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    // ── Title ───────────────────────────────────────────────────────────────
    titleLabel.setText ("FUZZAVER", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, kText);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // ── Global toggles ──────────────────────────────────────────────────────
    setupToggle (wavToggle,  "Test File Playback");
    setupToggle (fuzzToggle, "Fuzz");

    if (p.useWavFileParam)
        wavAttach  = std::make_unique<juce::ButtonParameterAttachment> (*p.useWavFileParam,  wavToggle);
    if (p.fuzzEnabledParam)
        fuzzAttach = std::make_unique<juce::ButtonParameterAttachment> (*p.fuzzEnabledParam, fuzzToggle);

    // ── Compressor ──────────────────────────────────────────────────────────
    compSectionLabel.setText ("COMPRESSOR", juce::dontSendNotification);
    compSectionLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    compSectionLabel.setColour (juce::Label::textColourId, kComp);
    addAndMakeVisible (compSectionLabel);

    setupToggle (compEnabledToggle, "On");
    if (p.compEnabledParam)
        compEnabledAttach = std::make_unique<juce::ButtonParameterAttachment> (*p.compEnabledParam, compEnabledToggle);

    setupRotary (compThreshSlider,  compThreshLabel,  "Threshold", kComp);
    setupRotary (compRatioSlider,   compRatioLabel,   "Ratio",     kComp);
    setupRotary (compKneeSlider,    compKneeLabel,    "Knee",      kComp);
    setupRotary (compAttackSlider,  compAttackLabel,  "Attack",    kComp);
    setupRotary (compReleaseSlider, compReleaseLabel, "Release",   kComp);
    setupRotary (compMakeupSlider,  compMakeupLabel,  "Makeup",    kComp);

    if (p.compThreshParam)
        compThreshAttach   = std::make_unique<juce::SliderParameterAttachment> (*p.compThreshParam,   compThreshSlider);
    if (p.compRatioParam)
        compRatioAttach    = std::make_unique<juce::SliderParameterAttachment> (*p.compRatioParam,    compRatioSlider);
    if (p.compKneeParam)
        compKneeAttach     = std::make_unique<juce::SliderParameterAttachment> (*p.compKneeParam,     compKneeSlider);
    if (p.compAttackParam)
        compAttackAttach   = std::make_unique<juce::SliderParameterAttachment> (*p.compAttackParam,   compAttackSlider);
    if (p.compReleaseParam)
        compReleaseAttach  = std::make_unique<juce::SliderParameterAttachment> (*p.compReleaseParam,  compReleaseSlider);
    if (p.compMakeupParam)
        compMakeupAttach   = std::make_unique<juce::SliderParameterAttachment> (*p.compMakeupParam,   compMakeupSlider);

    // ── Fuzz / TS9 ──────────────────────────────────────────────────────────
    fuzzSectionLabel.setText ("FUZZ (TS9)", juce::dontSendNotification);
    fuzzSectionLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    fuzzSectionLabel.setColour (juce::Label::textColourId, kFuzz);
    addAndMakeVisible (fuzzSectionLabel);

    setupRotary (ts9DriveSlider, ts9DriveLabel, "Drive", kFuzz);
    setupRotary (ts9LevelSlider, ts9LevelLabel, "Level", kFuzz);
    setupRotary (ts9ToneSlider,  ts9ToneLabel,  "Tone",  kFuzz);

    if (auto* dp = dynamic_cast<juce::AudioParameterFloat*> (findByID (p, "ts9_drive")))
        ts9DriveAttach = std::make_unique<juce::SliderParameterAttachment> (*dp, ts9DriveSlider);
    if (auto* lp = dynamic_cast<juce::AudioParameterFloat*> (findByID (p, "ts9_level")))
        ts9LevelAttach = std::make_unique<juce::SliderParameterAttachment> (*lp, ts9LevelSlider);
    if (auto* tp = dynamic_cast<juce::AudioParameterFloat*> (findByID (p, "ts9_tone")))
        ts9ToneAttach  = std::make_unique<juce::SliderParameterAttachment> (*tp, ts9ToneSlider);

    // ── Mix levels ──────────────────────────────────────────────────────────
    mixSectionLabel.setText ("MIX LEVELS", juce::dontSendNotification);
    mixSectionLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    mixSectionLabel.setColour (juce::Label::textColourId, kMix);
    addAndMakeVisible (mixSectionLabel);

    auto initLevelSlider = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
        s.setColour (juce::Slider::trackColourId,           kMix);
        s.setColour (juce::Slider::backgroundColourId,      kPanel.brighter (0.2f));
        s.setColour (juce::Slider::thumbColourId,           kText);
        s.setColour (juce::Slider::textBoxTextColourId,     kSub);
        s.setColour (juce::Slider::textBoxOutlineColourId,  juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (11.0f));
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, kSub);
        addAndMakeVisible (l);
    };

    initLevelSlider (downShiftSlider, downShiftLabel, "Down Shift (octave)");
    initLevelSlider (upShiftSlider,   upShiftLabel,   "Up Shift  (octave)");

    if (p.downShiftLevelParam)
        downShiftAttach = std::make_unique<juce::SliderParameterAttachment> (*p.downShiftLevelParam, downShiftSlider);
    if (p.upShiftLevelParam)
        upShiftAttach   = std::make_unique<juce::SliderParameterAttachment> (*p.upShiftLevelParam,   upShiftSlider);

    // ── Solo buttons ────────────────────────────────────────────────────────
    auto setupSolo = [&] (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId,   kPanel.brighter (0.25f));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe8a020));
        b.setColour (juce::TextButton::textColourOffId,  kSub);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        addAndMakeVisible (b);
    };
    setupSolo (downSoloButton);
    setupSolo (upSoloButton);

    if (p.downShiftSoloParam)
        downSoloAttach = std::make_unique<juce::ButtonParameterAttachment> (*p.downShiftSoloParam, downSoloButton);
    if (p.upShiftSoloParam)
        upSoloAttach   = std::make_unique<juce::ButtonParameterAttachment> (*p.upShiftSoloParam,   upSoloButton);

    setSize (500, 560);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Draw section panel backgrounds
    auto drawPanel = [&] (juce::Rectangle<int> r, juce::Colour accent)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (accent.withAlpha (0.5f));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.0f);
    };

    drawPanel (compPanelBounds,  kComp);
    drawPanel (fuzzPanelBounds,  kFuzz);
    drawPanel (mixPanelBounds,   kMix);
}

void AudioPluginAudioProcessorEditor::resized()
{
    const int pad   = 8;
    const int gap   = 6;
    auto area = getLocalBounds().reduced (pad);

    // ── Header ──────────────────────────────────────────────────────────────
    auto header = area.removeFromTop (40);
    titleLabel.setBounds (header.removeFromLeft (160));
    wavToggle.setBounds  (header.removeFromRight (160));
    area.removeFromTop (gap);

    // ── Compressor panel ────────────────────────────────────────────────────
    compPanelBounds = area.removeFromTop (210).reduced (0, 2);
    {
        auto inner = compPanelBounds.reduced (8, 6);

        auto hdr = inner.removeFromTop (24);
        compSectionLabel.setBounds (hdr.removeFromLeft (120));
        compEnabledToggle.setBounds (hdr.removeFromLeft (44));
        inner.removeFromTop (4);

        auto row1 = inner.removeFromTop (84);
        layoutKnobRow (row1, { {&compThreshSlider, &compThreshLabel},
                                {&compRatioSlider,  &compRatioLabel},
                                {&compKneeSlider,   &compKneeLabel} });
        inner.removeFromTop (4);
        auto row2 = inner.removeFromTop (84);
        layoutKnobRow (row2, { {&compAttackSlider,  &compAttackLabel},
                                {&compReleaseSlider, &compReleaseLabel},
                                {&compMakeupSlider,  &compMakeupLabel} });
    }
    area.removeFromTop (gap);

    // ── Fuzz panel ──────────────────────────────────────────────────────────
    fuzzPanelBounds = area.removeFromTop (120).reduced (0, 2);
    {
        auto inner = fuzzPanelBounds.reduced (8, 6);

        auto hdr = inner.removeFromTop (24);
        fuzzSectionLabel.setBounds (hdr.removeFromLeft (120));
        fuzzToggle.setBounds (hdr.removeFromLeft (60));
        inner.removeFromTop (4);

        auto row = inner.removeFromTop (84);
        layoutKnobRow (row, { {&ts9DriveSlider, &ts9DriveLabel},
                               {&ts9LevelSlider, &ts9LevelLabel},
                               {&ts9ToneSlider,  &ts9ToneLabel} });
    }
    area.removeFromTop (gap);

    // ── Mix panel ───────────────────────────────────────────────────────────
    mixPanelBounds = area.removeFromTop (140).reduced (0, 2);
    {
        auto inner = mixPanelBounds.reduced (8, 6);

        auto hdr = inner.removeFromTop (22);
        mixSectionLabel.setBounds (hdr);
        inner.removeFromTop (4);

        const int labelH  = 16;
        const int sliderH = 26;
        const int rowH    = labelH + sliderH + 2;
        const int soloW   = 60;

        auto r1 = inner.removeFromTop (rowH);
        downShiftLabel.setBounds  (r1.removeFromTop (labelH));
        downSoloButton.setBounds  (r1.removeFromRight (soloW).reduced (0, 2));
        downShiftSlider.setBounds (r1);

        inner.removeFromTop (4);
        auto r2 = inner.removeFromTop (rowH);
        upShiftLabel.setBounds  (r2.removeFromTop (labelH));
        upSoloButton.setBounds  (r2.removeFromRight (soloW).reduced (0, 2));
        upShiftSlider.setBounds (r2);
    }
}
