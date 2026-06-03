#pragma once
#include <JuceHeader.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();

    // Rotary knob
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float startAngle, float endAngle,
                          juce::Slider& slider) override;

    // Toggle button (enable / bypass)
    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool isMouseOverButton,
                          bool isButtonDown) override;

    // ComboBox background
    void drawComboBox(juce::Graphics& g, int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY,
                      int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box,
                              juce::Label& label) override;

    // Label
    juce::Font getLabelFont(juce::Label&) override;

    // Colour palette
    static juce::Colour background()   { return juce::Colour(0xFF18181F); }
    static juce::Colour panelBg()      { return juce::Colour(0xFF22222B); }
    static juce::Colour panelBorder()  { return juce::Colour(0xFF38383F); }
    static juce::Colour accent()       { return juce::Colour(0xFFE07A40); }
    static juce::Colour knobTrack()    { return juce::Colour(0xFF303038); }
    static juce::Colour textPrimary()  { return juce::Colour(0xFFCCCCCC); }
    static juce::Colour textSecond()   { return juce::Colour(0xFF888890); }
};
