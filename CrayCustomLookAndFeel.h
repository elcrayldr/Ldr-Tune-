#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class CrayCustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrayCustomLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfffacc15));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xfffacc15));
        setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff12151b));
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff27272a));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff181b22));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xfffacc15));
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffa1a1aa));
        setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        setColour (juce::ToggleButton::tickColourId, juce::Colour (0xfffacc15));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const auto center = bounds.getCentre();

        g.setColour (juce::Colour (0xff1f242d));
        juce::Path bgArc;
        bgArc.addCentredArc (center.x, center.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xfffacc15));
        juce::Path activeArc;
        activeArc.addCentredArc (center.x, center.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, toAngle, true);
        g.strokePath (activeArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xff12151b));
        g.fillEllipse (center.x - radius + 8.0f, center.y - radius + 8.0f, (radius - 8.0f) * 2.0f, (radius - 8.0f) * 2.0f);

        juce::Path needle;
        const auto needleLen = radius - 10.0f;
        needle.addLineSegment (juce::Line<float> (center.x, center.y,
                                                  center.x + needleLen * std::sin (toAngle),
                                                  center.y - needleLen * std::cos (toAngle)), 2.5f);
        g.setColour (juce::Colour (0xfffacc15));
        g.strokePath (needle, juce::PathStrokeType (2.5f));
    }
};
