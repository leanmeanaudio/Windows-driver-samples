#include "PluginEditor.h"

LAMAConnectACXAudioProcessorEditor::LAMAConnectACXAudioProcessorEditor(LAMAConnectACXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Initialize test tone controls
    testToneButton.setToggleState(p.getTestToneEnabled(), juce::dontSendNotification);
    testToneButton.onClick = [this] {
        audioProcessor.enableTestTone(testToneButton.getToggleState());
    };
    testToneButton.setButtonText("Test Tone");
    testToneButton.setTooltip("Generate test sine wave for testing audio routing");
    addAndMakeVisible(testToneButton);

    frequencySlider.setRange(20.0, 20000.0, 1.0);
    frequencySlider.setValue(p.getTestToneFrequency());
    frequencySlider.setSkewFactorFromMidPoint(1000.0); // Logarithmic scale
    frequencySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.onValueChange = [this] {
        audioProcessor.setTestToneFrequency((float)frequencySlider.getValue());
    };
    addAndMakeVisible(frequencySlider);

    gainSlider.setRange(0.0, 1.0, 0.01);
    gainSlider.setValue(p.getTestToneGain());
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    gainSlider.setNumDecimalPlacesToDisplay(2);
    gainSlider.onValueChange = [this] {
        audioProcessor.setTestToneGain((float)gainSlider.getValue());
    };
    addAndMakeVisible(gainSlider);

    // Labels for sliders
    frequencyLabel.setText("Frequency:", juce::dontSendNotification);
    frequencyLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(frequencyLabel);
    
    gainLabel.setText("Gain:", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(gainLabel);

    // Host audio info labels
    sampleRateLabel.setFont(juce::Font(14.0f));
    sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    addAndMakeVisible(sampleRateLabel);
    
    sampleRateDisplay.setFont(juce::Font(14.0f));
    sampleRateDisplay.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sampleRateDisplay);

    bufferSizeLabel.setFont(juce::Font(14.0f));
    bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
    addAndMakeVisible(bufferSizeLabel);
    
    bufferSizeDisplay.setFont(juce::Font(14.0f));
    bufferSizeDisplay.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bufferSizeDisplay);

    channelLayoutLabel.setFont(juce::Font(14.0f));
    channelLayoutLabel.setText("Plugin Channels:", juce::dontSendNotification);
    addAndMakeVisible(channelLayoutLabel);
    
    // Channel layout combo
    channelLayoutCombo.addItemList(audioProcessor.getChannelLayoutParam()->choices, 1);
    channelLayoutCombo.setSelectedItemIndex(audioProcessor.getChannelLayoutIndex());
    channelLayoutCombo.setTooltip("Select how many channels the plugin should use (driver always uses 16 internally)");
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
                updateDriverButtonText();
                statusLabel.setText("Driver Status: Initialized", juce::dontSendNotification);
            } else {
                statusLabel.setText("Driver Status: " + audioProcessor.getLastErrorMessage(), juce::dontSendNotification);
            }
        } else {
            audioProcessor.stopDriverAudio();
            updateDriverButtonText();
            statusLabel.setText("Driver Status: Stopped", juce::dontSendNotification);
        }
    };
    addAndMakeVisible(driverButton);

    // Driver instance selection
    driverInstanceLabel.setText("Driver Instance:", juce::dontSendNotification);
    driverInstanceLabel.setTooltip("Select which virtual audio device to connect to (0-3)");
    addAndMakeVisible(driverInstanceLabel);
    
    driverInstanceCombo.addItem("Instance 0", 1);
    driverInstanceCombo.addItem("Instance 1", 2);
    driverInstanceCombo.addItem("Instance 2", 3);
    driverInstanceCombo.addItem("Instance 3", 4);
    driverInstanceCombo.setSelectedItemIndex(p.getDriverInstanceIndex());
    driverInstanceCombo.setTooltip("Each instance creates a separate virtual audio device");
    driverInstanceCombo.onChange = [this] {
        int idx = driverInstanceCombo.getSelectedItemIndex();
        if (audioProcessor.setDriverInstanceIndex(idx)) {
            statusLabel.setText("Switched to driver instance " + juce::String(idx), juce::dontSendNotification);
            updateDriverButtonText();
        } else {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Driver Instance",
                                                   "Failed to switch to instance " + juce::String(idx) + 
                                                   "\n\nCheck if the driver is installed and the instance is available.");
        }
    };
    addAndMakeVisible(driverInstanceCombo);

    // Status components
    statusLabel.setFont(juce::Font(14.0f));
    statusLabel.setTooltip("Current driver connection and processing status");
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(statusPanel);
    addAndMakeVisible(statusIndicator);

    // Level meter labels
    inputLabel.setText("Input Levels", juce::dontSendNotification);
    inputLabel.setJustificationType(juce::Justification::centred);
    inputLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(inputLabel);
    
    outputLabel.setText("Output Levels", juce::dontSendNotification);
    outputLabel.setJustificationType(juce::Justification::centred);
    outputLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(outputLabel);
    
    addAndMakeVisible(meterBackground);
    
    // Create meter components (16 max, matching driver capability)
    inputMeters.reserve(16);
    outputMeters.reserve(16);
    for (int i = 0; i < 16; ++i) {
        MeterComponent* inMeter = new MeterComponent();
        MeterComponent* outMeter = new MeterComponent();
        inMeter->setTooltip("Input level for channel " + juce::String(i + 1));
        outMeter->setTooltip("Output level for channel " + juce::String(i + 1));
        addAndMakeVisible(inMeter);
        addAndMakeVisible(outMeter);
        inputMeters.push_back(inMeter);
        outputMeters.push_back(outMeter);
    }

    // Initialize status
    updateDriverButtonText();
    
    // Set window size and start refresh timer
    setSize(650, 750);
    startTimer(50); // 20 Hz refresh rate
    
    // Set initial focus
    driverButton.grabKeyboardFocus();
}

LAMAConnectACXAudioProcessorEditor::~LAMAConnectACXAudioProcessorEditor() {
    // Clean up meter components
    for (auto* m : inputMeters) delete m;
    for (auto* m : outputMeters) delete m;
}

void LAMAConnectACXAudioProcessorEditor::paint(juce::Graphics& g) {
    // Gradient background
    juce::ColourGradient gradient(juce::Colour(0xff2a2a2a), 0, 0,
                                  juce::Colour(0xff1a1a1a), 0, (float)getHeight(), false);
    g.setGradientFill(gradient);
    g.fillAll();
    
    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("LAMAConnect ACX Virtual Audio", getLocalBounds().removeFromTop(35), 
               juce::Justification::centred);
}

void LAMAConnectACXAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(40); // Space for title

    // Control section (test tone + host info)
    auto controlsArea = area.removeFromTop(280);
    auto leftColumn = controlsArea.removeFromLeft(controlsArea.getWidth() / 2).reduced(5);
    auto rightColumn = controlsArea.reduced(5);
    const int controlH = 28, gap = 8;

    // Left column: Test tone controls
    auto testToneSection = leftColumn;
    testToneButton.setBounds(testToneSection.removeFromTop(controlH));
    testToneSection.removeFromTop(gap);
    
    auto freqRow = testToneSection.removeFromTop(controlH);
    frequencyLabel.setBounds(freqRow.removeFromLeft(80));
    frequencySlider.setBounds(freqRow);
    
    testToneSection.removeFromTop(gap);
    
    auto gainRow = testToneSection.removeFromTop(controlH);
    gainLabel.setBounds(gainRow.removeFromLeft(80));
    gainSlider.setBounds(gainRow);

    // Right column: Host info and driver control
    auto hostSection = rightColumn;
    
    // Sample rate row
    auto srArea = hostSection.removeFromTop(controlH);
    sampleRateLabel.setBounds(srArea.removeFromLeft(100));
    sampleRateDisplay.setBounds(srArea);
    
    hostSection.removeFromTop(gap);
    
    // Buffer size row
    auto bsArea = hostSection.removeFromTop(controlH);
    bufferSizeLabel.setBounds(bsArea.removeFromLeft(100));
    bufferSizeDisplay.setBounds(bsArea);
    
    hostSection.removeFromTop(gap);
    
    // Channel layout row
    auto chArea = hostSection.removeFromTop(controlH);
    channelLayoutLabel.setBounds(chArea.removeFromLeft(120));
    channelLayoutCombo.setBounds(chArea.withSizeKeepingCentre(150, controlH));
    
    hostSection.removeFromTop(gap * 2);
    
    // Driver button
    driverButton.setBounds(hostSection.removeFromTop(controlH * 2));

    area.removeFromTop(gap);

    // Driver instance selection
    auto instanceArea = area.removeFromTop(controlH * 2);
    auto instanceLabelArea = instanceArea.removeFromTop(controlH);
    driverInstanceLabel.setBounds(instanceLabelArea.removeFromLeft(120));
    auto comboArea = instanceArea.removeFromTop(controlH);
    driverInstanceCombo.setBounds(comboArea.withSizeKeepingCentre(150, controlH));

    area.removeFromTop(gap);

    // Status area
    auto statusArea = area.removeFromTop(40);
    statusPanel.setBounds(statusArea.removeFromLeft(140));
    statusIndicator.setBounds(statusPanel.getX() + 5, statusPanel.getY() + 10, 20, 20);
    statusLabel.setBounds(statusIndicator.getRight() + 10, statusPanel.getY() + 5, 
                         statusArea.getWidth() - 150, 30);

    area.removeFromTop(gap);

    // Level meters area
    auto metersArea = area;
    
    // Meter labels
    auto meterLabelsArea = metersArea.removeFromTop(30);
    inputLabel.setBounds(meterLabelsArea.removeFromLeft(meterLabelsArea.getWidth() / 2));
    outputLabel.setBounds(meterLabelsArea);
    
    // Meter background
    meterBackground.setBounds(metersArea);

    // Position meter bars based on active channel count
    int numVisibleChannels = audioProcessor.getChannelCountForLayout();
    if (numVisibleChannels < 2) numVisibleChannels = 2;
    
    float meterWidth = metersArea.getWidth() / float(numVisibleChannels * 2);
    float meterHeight = (float)metersArea.getHeight();
    float meterGap = meterWidth * 0.15f;
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
        statusLabel.setText("Driver Status: Disconnected", juce::dontSendNotification);
        statusIndicator.setStatus(StatusIndicator::Off);
    }
    
    updateDriverButtonText();

    // Update format displays
    double sampleRate = audioProcessor.getSampleRate();
    int blockSize = audioProcessor.getBlockSize();
    
    if (sampleRate > 0) {
        sampleRateDisplay.setText(juce::String(sampleRate, 0) + " Hz", juce::dontSendNotification);
    } else {
        sampleRateDisplay.setText("Not set", juce::dontSendNotification);
    }
    
    if (blockSize > 0) {
        bufferSizeDisplay.setText(juce::String(blockSize) + " samples", juce::dontSendNotification);
    } else {
        bufferSizeDisplay.setText("Not set", juce::dontSendNotification);
    }

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
            else if (levelIn > 0.5f) inColour = juce::Colours::yellow;
            
            juce::Colour outColour = juce::Colours::green;
            if (levelOut > 0.9f) outColour = juce::Colours::red;
            else if (levelOut > 0.7f) outColour = juce::Colours::orange;
            else if (levelOut > 0.5f) outColour = juce::Colours::yellow;
            
            inputMeters[i]->setMeterColour(inColour);
            outputMeters[i]->setMeterColour(outColour);
            inputMeters[i]->setLevel(levelIn);
            outputMeters[i]->setLevel(levelOut);
        } else {
            // Inactive channels
            inputMeters[i]->setMeterColour(juce::Colours::darkgrey.darker());
            outputMeters[i]->setMeterColour(juce::Colours::darkgrey.darker());
            inputMeters[i]->setLevel(0.0f);
            outputMeters[i]->setLevel(0.0f);
        }
    }
    
    // Update test tone controls from parameters
    bool paramToneEnabled = (bool)*audioProcessor.getTestToneParam();
    if (testToneButton.getToggleState() != paramToneEnabled) {
        testToneButton.setToggleState(paramToneEnabled, juce::dontSendNotification);
    }
    
    float paramFreq = *audioProcessor.getTestToneFreqParam();
    if (std::abs(frequencySlider.getValue() - paramFreq) > 1.0) {
        frequencySlider.setValue(paramFreq, juce::dontSendNotification);
    }
    
    float paramGain = *audioProcessor.getTestToneGainParam();
    if (std::abs(gainSlider.getValue() - paramGain) > 0.01) {
        gainSlider.setValue(paramGain, juce::dontSendNotification);
    }
    
    // Update channel layout combo
    int paramLayoutIndex = audioProcessor.getChannelLayoutIndex();
    if (channelLayoutCombo.getSelectedItemIndex() != paramLayoutIndex) {
        channelLayoutCombo.setSelectedItemIndex(paramLayoutIndex, juce::dontSendNotification);
    }
}

void LAMAConnectACXAudioProcessorEditor::updateDriverButtonText() {
    bool initialized = audioProcessor.isDriverInitialized();
    bool active = audioProcessor.isProcessingActive();
    
    if (initialized) {
        if (active) {
            driverButton.setButtonText("Stop Driver");
            driverButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        } else {
            driverButton.setButtonText("Driver Ready");
            driverButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
        }
    } else {
        driverButton.setButtonText("Initialize Driver");
        driverButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    }
}