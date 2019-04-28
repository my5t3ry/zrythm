/*
  ==============================================================================

    This file was auto-generated by the Jucer!

    It contains the basic startup code for a Juce application.

  ==============================================================================
*/

#ifndef __VOLUMEDIAL_H_CFF4EBB1__
#define __VOLUMEDIAL_H_CFF4EBB1__

#include "JuceHeader.h"
#include "JucePluginCharacteristics.h"
#include "../Binary Data/UI/knobs.h"

//==============================================================================
/**
*/
class Knob : public Slider
{
public:
    //==============================================================================
    Knob();
    ~Knob() override;

    String getTextFromValue (double value) override;
    void setReadoutType (String type);

    void paint (Graphics& g) override;

private:
    //==============================================================================
    Image knobImage, smallKnobImage;
    int frameWidth;
    String readoutType;
    String testReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Knob);
};

#endif  // __VOLUMEDIAL_H_CFF4EBB1__