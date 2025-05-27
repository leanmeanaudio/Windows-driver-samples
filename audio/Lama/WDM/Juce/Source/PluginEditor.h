#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Simple meter bar component
class MeterComponent : public juce::Component {
public:
    MeterComponent() { setOpaque(true); }
    
    void setMeterColour(const juce::Colour& newColour) {
        if (mColour != newColour) {
            mColour = newColour;
            repaint();
        }
    }
    
    void setActive(bool shouldBeActive) {
        if (mActive != shouldBeActive) {
            mActive = shouldBeActive;
            repaint();
        }
    }
    
    void paint(juce::Graphics& g) override {
        if (mActive)
            g.fillAll(mColour);
        else
            g.fillAll(juce::Colours::darkgrey.darker());
    }
    
private:
    juce::Colour mColour = juce::Colours::green;
    bool mActive = false;
};

// Status panel background
class StatusPanel : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::darkgrey.withAlpha(0.7f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds, 6.0f, 2.0f);
    }
};

// Status LED indicator
class StatusIndicator : public juce::Component {
public:
    enum Status { Off, Initialized, Active };
    
    StatusIndicator() : currentStatus(Off) {}
    
    void setStatus(Status newStatus) {
        if (currentStatus != newStatus) {
            currentStatus = newStatus;
            repaint();
        }
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().reduced(2).toFloat();
        
        // Background
        g.setColour(juce::Colours::darkgrey);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // LED color
        auto indicatorBounds = bounds.reduced(3);
        switch (currentStatus) {
            case Off:         g.setColour(juce::Colours::red);    break;
            case Initialized: g.setColour(juce::Colours::yellow); break;
            case Active:      g.setColour(juce::Colours::green);  break;
        }
        
        // Draw LED
        g.fillEllipse(indicatorBounds);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawEllipse(indicatorBounds, 1.5f);
    }
    
private:
    Status currentStatus;
};

//==============================================================================
class LAMAConnectACXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer {
public:
    explicit LAMAConnectACXAudioProcessorEditor(LAMAConnectACXAudioProcessor& p);
    ~LAMAConnectACXAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateDriverButtonText();

    LAMAConnectACXAudioProcessor& audioProcessor;

    // Test tone controls
    juce::ToggleButton testToneButton { "Test Tone" };
    juce::Slider frequencySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // Host info displays
    juce::Label sampleRateLabel;
    juce::Label sampleRateDisplay;
    juce::Label bufferSizeLabel;
    juce::Label bufferSizeDisplay;
    juce::Label channelLayoutLabel;
    juce::ComboBox channelLayoutCombo;

    // Driver controls
    juce::TextButton driverButton { "Initialize Driver" };

    // Driver instance selection
    juce::Label driverInstanceLabel;
    juce::ComboBox driverInstanceCombo;

    // Status display
    StatusPanel statusPanel;
    StatusIndicator statusIndicator;
    juce::Label statusLabel;

    // Level meters
    juce::Label inputLabel;
    juce::Label outputLabel;
    
    class MeterBackgroundComponent : public juce::Component {
        void paint(juce::Graphics& g) override { 
            g.fillAll(juce::Colours::darkgrey); 
        }
    } meterBackground;
    
    std::vector<MeterComponent*> inputMeters;
    std::vector<MeterComponent*> outputMeters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LAMAConnectACXAudioProcessorEditor)
};