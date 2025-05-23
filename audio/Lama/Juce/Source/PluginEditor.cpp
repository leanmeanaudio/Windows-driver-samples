#include "PluginEditor.h"

LAMAConnectACXAudioProcessorEditor::LAMAConnectACXAudioProcessorEditor(LAMAConnectACXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Initialize test tone controls
    testToneButton.setToggleState(p.getTestToneEnabled(), juce::dontSendNotification);
    testToneButton.onClick = [this] {
        audioProcessor.enableTestTone(testToneButton.getToggleState());
    };
    addAndMakeVisible(testToneButton);

    frequencySlider.setRange(20.0, 20000.0, 1.0);
    frequencySlider.setValue(p.getTestToneFrequency());
    frequencySlider.onValueChange = [this] {
        audioProcessor.setTestToneFrequency((float)frequencySlider.getValue());
    };
    addAndMakeVisible(frequencySlider);

    gainSlider.setRange(0.0, 1.0, 0.01);
    gainSlider.setValue(p.getTestToneGain());
    gainSlider.onValueChange = [this] {
        audioProcessor.setTestToneGain((float)gainSlider.getValue());
    };
    addAndMakeVisible(gainSlider);

    // Host audio info labels
    sampleRateLabel.setFont(juce::Font(14.0f));
    sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    addAndMakeVisible(sampleRateLabel);
    
    sampleRateDisplay.setFont(juce::Font(14.0f));
    addAndMakeVisible(sampleRateDisplay);

    bufferSizeLabel.setFont(juce::Font(14.0f));
    bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
    addAndMakeVisible(bufferSizeLabel);
    
    bufferSizeDisplay.setFont(juce::Font(14.0f));
    addAndMakeVisible(bufferSizeDisplay);

    channelLayoutLabel.setFont(juce::Font(14.0f));
    channelLayoutLabel.setText("Plugin Channels:", juce::dontSendNotification);
    addAndMakeVisible(channelLayoutLabel);
    
    // Channel layout combo
    channelLayoutCombo.addItemList(audioProcessor.getChannelLayoutParam()->choices, 1);
    channelLayoutCombo.setSelectedItemIndex(audioProcessor.getChannelLayoutIndex());
    channelLayoutCombo.onChange = [this] {
        int newIndex = channelLayoutCombo.getSelectedItemIndex();
        *audioProcessor.getChannelLayoutParam() = newIndex;
        // Note: Driver always uses 16 channels, this just changes how many the plugin uses
    };
    addAndMakeVisible(channelLayoutCombo);

    // Driver control button
    driverButton.onClick = [this] {
        if (!audioProcessor.isDriverInitialized()) {
            if (audioProcessor.initializeDriver()) {
                driverButton.setButtonText("Stop Driver");
                statusLabel.setText("Driver Status: Initialized", juce::dontSendNotification);
            } else {
                statusLabel.setText("Driver Status: " + audioProcessor.getLastErrorMessage(), juce::dontSendNotification);
            }
        } else {
            audioProcessor.stopDriverAudio();
            driverButton.setButtonText("Initialize Driver");
            statusLabel.setText("Driver Status: Stopped", juce::dontSendNotification);
        }
    };
    addAndMakeVisible(driverButton);

    // Driver instance selection
    driverInstanceLabel.setText("Driver Instance:", juce::dontSendNotification);
    driverInstanceCombo.addItem("Instance 0", 1);
    driverInstanceCombo.addItem("Instance 1", 2);
    driverInstanceCombo.addItem("Instance 2", 3);
    driverInstanceCombo.addItem("Instance 3", 4);
    driverInstanceCombo.setSelectedItemIndex(p.getDriverInstanceIndex());
    driverInstanceCombo.onChange = [this] {
        int idx = driverInstanceCombo.getSelectedItemIndex();
        if (audioProcessor.setDriverInstanceIndex(idx)) {
            statusLabel.setText("Switched to driver instance " + juce::String(idx), juce::dontSendNotification);
            updateDriverButtonText();
        } else {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Driver Instance",
                                                   "Failed to switch to instance " + juce::String(idx));
        }
    };
    addAndMakeVisible(driverInstanceLabel);
    addAndMakeVisible(driverInstanceCombo);

    // Status components
    statusLabel.setFont(juce::Font(14.0f));
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(statusPanel);
    addAndMakeVisible(statusIndicator);

    // Level meter labels
    inputLabel.setText("Input Levels", juce::dontSendNotification);
    inputLabel.setJustificationType(juce::Justification::centred);
    outputLabel.setText("Output Levels", juce::dontSendNotification);
    outputLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputLabel);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(meterBackground);
    
    // Create meter components (16 max, matching driver capability)
    inputMeters.reserve(16);
    outputMeters.reserve(16);
    for (int i = 0; i < 16; ++i) {
        MeterComponent* inMeter = new MeterComponent();
        MeterComponent* outMeter = new MeterComponent();
        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);
        inputMeters.push_back(inMeter);
        outputMeters.push_back(outMeter);
    }

    // Set window size and start refresh timer
    setSize(600, 700);
    startTimer(50); // 20 Hz refresh rate
}

LAMAConnectACXAudioProcessorEditor::~LAMAConnectACXAudioProcessorEditor() {
    for (auto* m : inputMeters) delete m;
    for (auto* m : outputMeters) delete m;
}

void LAMAConnectACXAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

void LAMAConnectACXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(10);

    // Control section (test tone + host info)
    auto controlsArea = area.removeFromTop(240);
    auto leftColumn = controlsArea.removeFromLeft(controlsArea.getWidth() / 2).reduced(5);
    auto rightColumn = controlsArea.reduced(5);
    const int controlH = 24, gap = 8;

    // Left column: Test tone controls
    auto testToneSection = leftColumn.removeFromTop(controlH * 4);
    testToneButton.setBounds(testToneSection.removeFromTop(controlH));
    testToneSection.removeFromTop(gap);
    frequencySlider.setBounds(testToneSection.removeFromTop(controlH));
    gainSlider.setBounds(testToneSection.removeFromTop(controlH));

    // Right column: Host info and driver control
    auto hostSection = rightColumn.removeFromTop(controlH * 5);
    
    // Sample rate row
    auto srArea = hostSection.removeFromTop(controlH);
    sampleRateLabel.setBounds(srArea.removeFromLeft(srArea.getWidth() / 2));
    sampleRateDisplay.setBounds(srArea);
    
    // Buffer size row
    auto bsArea = hostSection.removeFromTop(controlH);
    bufferSizeLabel.setBounds(bsArea.removeFromLeft(bsArea.getWidth() / 2));
    bufferSizeDisplay.setBounds(bsArea);
    
    hostSection.removeFromTop(gap);
    
    // Channel layout row
    auto chArea = hostSection.removeFromTop(controlH);
    channelLayoutLabel.setBounds(chArea.removeFromLeft(chArea.getWidth() / 3));
    channelLayoutCombo.setBounds(chArea);
    
    hostSection.removeFromTop(gap);
    
    // Driver button
    driverButton.setBounds(hostSection.removeFromTop(controlH));

    area.removeFromTop(10);

    // Driver instance selection
    auto instanceArea = area.removeFromTop(controlH * 2);
    auto instanceLabelArea = instanceArea.removeFromTop(controlH);
    driverInstanceLabel.setBounds(instanceLabelArea.removeFromLeft(110));
    auto comboArea = instanceArea.removeFromTop(controlH);
    driverInstanceCombo.setBounds(comboArea.withSizeKeepingCentre(150, controlH));

    area.removeFromTop(10);

    // Level meters area
    auto metersArea = area;
    
    // Meter labels
    auto meterLabelsArea = metersArea.removeFromTop(25);
    inputLabel.setBounds(meterLabelsArea.removeFromLeft(meterLabelsArea.getWidth() / 2));
    outputLabel.setBounds(meterLabelsArea);
    
    // Meter background
    meterBackground.setBounds(metersArea);

    // Position meter bars based on active channel count
    int numVisibleChannels = audioProcessor.getChannelCountForLayout();
    if (numVisibleChannels < 2) numVisibleChannels = 2;
    
    float meterWidth = metersArea.getWidth() / float(numVisibleChannels * 2);
    float meterHeight = (float)metersArea.getHeight();
    float meterGap = meterWidth * 0.1f;
    float actualMeterWidth = meterWidth - meterGap;
    
    // Position visible meters
    for (int i = 0; i < numVisibleChannels; ++i) {
        // Input meters (left half)
        inputMeters[i]->setBounds(
            metersArea.getX() + (int)(i * meterWidth) + (int)(meterGap / 2),
            metersArea.getY(),
            (int)actualMeterWidth, (int)meterHeight);
        inputMeters[i]->setVisible(true);
        inputMeters[i]->setActive(true);
        
        // Output meters (right half)
        outputMeters[i]->setBounds(
            metersArea.getX() + (int)(numVisibleChannels * meterWidth) + (int)(i * meterWidth) + (int)(meterGap / 2),
            metersArea.getY(),
            (int)actualMeterWidth, (int)meterHeight);
        outputMeters[i]->setVisible(true);
        outputMeters[i]->setActive(true);
    }
    
    // Hide unused meters
    for (int i = numVisibleChannels; i < 16; ++i) {
        inputMeters[i]->setVisible(false);
        inputMeters[i]->setActive(false);
        outputMeters[i]->setVisible(false);
        outputMeters[i]->setActive(false);
    }

    // Status panel and indicator
    statusPanel.setBounds(10, 10, 120, 30);
    statusIndicator.setBounds(statusPanel.getX() + 5, statusPanel.getY() + 5, 20, 20);
    statusLabel.setBounds(statusIndicator.getRight() + 5, statusPanel.getY() + 5, 200, 20);
}

void LAMAConnectACXAudioProcessorEditor::timerCallback() {
    // Update driver status
    bool initialized = audioProcessor.isDriverInitialized();
    bool active = audioProcessor.isProcessingActive();
    
    if (initialized && active) {
        statusLabel.setText("Driver Status: Active (16-ch internal)", juce::dontSendNotification);
        statusIndicator.setStatus(StatusIndicator::Active);
    } else if (initialized) {
        statusLabel.setText("Driver Status: Initialized", juce::dontSendNotification);
        statusIndicator.setStatus(StatusIndicator::Initialized);
    } else {
        statusLabel.setText("Driver Status: Stopped", juce::dontSendNotification);
        statusIndicator.setStatus(StatusIndicator::Off);
    }
    
    updateDriverButtonText();

    // Update format displays
    sampleRateDisplay.setText(juce::String(audioProcessor.getSampleRate()) + " Hz", juce::dontSendNotification);
    bufferSizeDisplay.setText(juce::String(audioProcessor.getBlockSize()) + " samples", juce::dontSendNotification);

    // Update level meters
    int numChannels = audioProcessor.getChannelCountForLayout();
    if (numChannels < 2) numChannels = 2;
    
    const auto& inLevels = audioProcessor.getInputLevels();
    const auto& outLevels = audioProcessor.getOutputLevels();
    
    for (int i = 0; i < 16; ++i) {
        bool channelActive = (i < numChannels);
        inputMeters[i]->setActive(channelActive);
        outputMeters[i]->setActive(channelActive);
        
        if (channelActive && active) {
            // Get RMS levels
            float levelIn = (i < inLevels.getNumChannels()) ? inLevels.getSample(i, 0) : 0.0f;
            float levelOut = (i < outLevels.getNumChannels()) ? outLevels.getSample(i, 0) : 0.0f;
            
            // Color based on level
            juce::Colour inColour = juce::Colours::green;
            if (levelIn > 0.9f) inColour = juce::Colours::red;
            else if (levelIn > 0.7f) inColour = juce::Colours::orange;
            
            juce::Colour outColour = juce::Colours::green;
            if (levelOut > 0.9f) outColour = juce::Colours::red;
            else if (levelOut > 0.7f) outColour = juce::Colours::orange;
            
            inputMeters[i]->setMeterColour(inColour);
            outputMeters[i]->setMeterColour(outColour);
        } else {
            // Inactive channels
            inputMeters[i]->setMeterColour(juce::Colours::darkgrey.darker());
            outputMeters[i]->setMeterColour(juce::Colours::darkgrey.darker());
        }
    }
}

void LAMAConnectACXAudioProcessorEditor::updateDriverButtonText() {
    bool initialized = audioProcessor.isDriverInitialized();
    
    if (initialized) {
        driverButton.setButtonText("Stop Driver");
    } else {
        driverButton.setButtonText("Initialize Driver");
    }
}