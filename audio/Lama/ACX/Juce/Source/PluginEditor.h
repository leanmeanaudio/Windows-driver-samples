#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// UI Components

/**
 * Simple meter bar component with level display
 */
class MeterComponent : public juce::Component {
public:
    MeterComponent() { setOpaque(true); }
    
    void setMeterColour(const juce::Colour& newColour) {
        if (mColour != newColour) {
            mColour = newColour;
            repaint();
        }
    }
    
    void setLevel(float newLevel) {
        newLevel = juce::jlimit(0.0f, 1.0f, newLevel);
        if (std::abs(mLevel - newLevel) > 0.001f) {
            mLevel = newLevel;
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
        auto bounds = getLocalBounds().toFloat();
        
        if (mActive) {
            // Background
            g.setColour(juce::Colours::black);
            g.fillRect(bounds);
            
            // Level bar
            float levelHeight = bounds.getHeight() * mLevel;
            juce::Rectangle<float> levelRect(bounds.getX(), bounds.getBottom() - levelHeight, 
                                           bounds.getWidth(), levelHeight);
            g.setColour(mColour);
            g.fillRect(levelRect);
            
            // Border
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawRect(bounds, 1.0f);
        } else {
            // Inactive state
            g.setColour(juce::Colours::darkgrey.darker());
            g.fillRect(bounds);
            g.setColour(juce::Colours::black);
            g.drawRect(bounds, 1.0f);
        }
    }
    
private:
    juce::Colour mColour = juce::Colours::green;
    float mLevel = 0.0f;
    bool mActive = false;
};

/**
 * Status panel background component
 */
class StatusPanel : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::darkgrey.withAlpha(0.8f));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
    }
};

/**
 * Status LED indicator component
 */
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
        
        // Background circle
        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(bounds);
        
        // LED color based on status
        auto indicatorBounds = bounds.reduced(3);
        switch (currentStatus) {
            case Off:         
                g.setColour(juce::Colours::red.darker());    
                break;
            case Initialized: 
                g.setColour(juce::Colours::yellow.brighter()); 
                break;
            case Active:      
                g.setColour(juce::Colours::green.brighter());  
                break;
        }
        
        // Draw LED with glow effect
        g.fillEllipse(indicatorBounds);
        
        // Add highlight for 3D effect
        if (currentStatus != Off) {
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            auto highlightBounds = indicatorBounds.reduced(2).translated(-1, -1);
            g.fillEllipse(highlightBounds.withSize(3, 3));
        }
        
        // Outer ring
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.drawEllipse(indicatorBounds, 1.5f);
    }
    
private:
    Status currentStatus;
};

/**
 * Meter background component
 */
class MeterBackgroundComponent : public juce::Component {
public:
    void paint(juce::Graphics& g) override { 
        g.setColour(juce::Colours::black);
        g.fillAll();
        
        // Add subtle grid lines
        g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
        auto bounds = getLocalBounds();
        
        // Horizontal lines for level references
        for (int i = 1; i < 4; ++i) {
            int y = bounds.getHeight() * i / 4;
            g.drawHorizontalLine(y, 0.0f, (float)bounds.getWidth());
        }
        
        // Vertical center line
        g.drawVerticalLine(bounds.getWidth() / 2, 0.0f, (float)bounds.getHeight());
    }
};

//==============================================================================
/**
 * Main plugin editor window
 * 
 * Provides a comprehensive interface for controlling the LAMAConnect virtual audio driver:
 * - Test tone generator with frequency and gain controls
 * - Driver connection management and instance selection
 * - Real-time level meters for all 16 channels
 * - Channel layout configuration
 * - Status display and error reporting
 */
class LAMAConnectACXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer {
public:
    explicit LAMAConnectACXAudioProcessorEditor(LAMAConnectACXAudioProcessor& p);
    ~LAMAConnectACXAudioProcessorEditor() override;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Timer callback for UI updates
    void timerCallback() override;
    
    /**
     * Update driver button text and color based on current state
     */
    void updateDriverButtonText();

    //==============================================================================
    // Reference to processor
    LAMAConnectACXAudioProcessor& audioProcessor;

    //==============================================================================
    // Test tone controls
    juce::ToggleButton testToneButton;
    juce::Slider frequencySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label frequencyLabel;
    juce::Label gainLabel;

    //==============================================================================
    // Host info displays
    juce::Label sampleRateLabel;
    juce::Label sampleRateDisplay;
    juce::Label bufferSizeLabel;
    juce::Label bufferSizeDisplay;
    juce::Label channelLayoutLabel;
    juce::ComboBox channelLayoutCombo;

    //==============================================================================
    // Driver controls
    juce::TextButton driverButton;

    // Driver instance selection
    juce::Label driverInstanceLabel;
    juce::ComboBox driverInstanceCombo;

    //==============================================================================
    // Status display
    StatusPanel statusPanel;
    StatusIndicator statusIndicator;
    juce::Label statusLabel;

    //==============================================================================
    // Level meters
    juce::Label inputLabel;
    juce::Label outputLabel;
    MeterBackgroundComponent meterBackground;
    
    std::vector<MeterComponent*> inputMeters;
    std::vector<MeterComponent*> outputMeters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LAMAConnectACXAudioProcessorEditor)
};