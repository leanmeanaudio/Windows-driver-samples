#include "PluginProcessor.h" // Will be PluginProcessor_NEW.h after rename by user
#include "PluginEditor.h"
#include "LAMAConnectInterface.h" // User will rename LAMAConnectInterface_NEW.h to this

//==============================================================================
LAMAConnectACXAudioProcessor::LAMAConnectACXAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // Initialize parameters
    testToneParam = new juce::AudioParameterBool("testTone", "Test Tone", false);
    addParameter(testToneParam);

    testToneFreqParam = new juce::AudioParameterFloat("testToneFreq", "Test Tone Frequency", 
                                                      juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 
                                                      440.0f);
    addParameter(testToneFreqParam);

    testToneGainParam = new juce::AudioParameterFloat("testToneGain", "Test Tone Gain", 
                                                     juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), 
                                                     -20.0f);
    addParameter(testToneGainParam);
    
    juce::StringArray channelChoices;
    channelChoices.add("Stereo (1-2)"); // Default, maps to 2 plugin channels
    channelChoices.add("8 Channels (1-8)");
    channelChoices.add("16 Channels (1-16)"); // Max driver channels
    // Add other layouts as needed, e.g., Quad, 5.1, 7.1, etc.
    // Ensure getChannelCountForLayout() maps these correctly.

    channelLayoutParam = new juce::AudioParameterChoice("channelLayout", "Channel Layout", channelChoices, 0);
    addParameter(channelLayoutParam);

    // Initialize DSP
    oscillator.setFrequency(testToneFreqParam->get());
    gain.setGainDecibels(testToneGainParam->get());

    // Initialize driver interface related members
    selectedDriverIndex = 0; // Default to first driver instance
    driverInitialized = false;
    processingActive = false;
    currentSampleRate = 0;
    currentBufferSize = 0;
    currentChannelCount = 2; // Default to stereo for plugin's perspective

    // IMPORTANT: Defer driverInterface.Initialize until prepareToPlay or a specific user action,
    // as we might not have all necessary info (like sample rate from host) yet.
    // initializeDriver(); // Don't call here, call in prepareToPlay or by user.
}

LAMAConnectACXAudioProcessor::~LAMAConnectACXAudioProcessor()
{
    // Ensure audio is stopped and driver is shut down
    if (processingActive) {
        stopDriverAudio(); // Internal method that calls driverInterface.stopAudio()
    }
    if (driverInitialized) {
        driverInterface.Shutdown();
        driverInitialized = false;
    }
}

//==============================================================================
const juce::String LAMAConnectACXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LAMAConnectACXAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LAMAConnectACXAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LAMAConnectACXAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double LAMAConnectACXAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LAMAConnectACXAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int LAMAConnectACXAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LAMAConnectACXAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String LAMAConnectACXAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void LAMAConnectACXAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void LAMAConnectACXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize DSP specs
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels(); // Use host output channels for DSP spec

    oscillator.prepare(spec);
    gain.prepare(spec);
    
    // Initialize level meter buffers
    // Using plugin's max output channels, not driver's 16.
    inputLevels.setSize(spec.numChannels, samplesPerBlock);
    outputLevels.setSize(spec.numChannels, samplesPerBlock);
    tempBuffer.setSize(spec.numChannels, samplesPerBlock); // For test tone generation

    // Initialize the driver if not already done
    if (!driverInitialized) {
        initializeDriver(); // This calls driverInterface.Initialize()
    }

    // (Re)Start audio with the driver using current (possibly new) settings
    // Determine the number of channels the plugin wants to use with the driver
    int channelsForDriver = getChannelCountForLayout();
    
    // Store these as they are the current active settings for the driver interaction
    currentSampleRate = sampleRate;
    currentBufferSize = samplesPerBlock;
    // currentChannelCount is updated by startDriverAudio on success

    if (driverInitialized) { // Only attempt to start if Initialize was successful
        if (!startDriverAudio(sampleRate, samplesPerBlock, channelsForDriver)) {
            // Host will be silenced by processBlock if processingActive is false
            lastErrorMessage = driverInterface.GetLastErrorMsg();
            juce::Logger::writeToLog("LAMAConnect: Failed to start driver audio in prepareToPlay: " + lastErrorMessage);
        }
    } else {
        processingActive = false; // Ensure not active if init failed
        juce::Logger::writeToLog("LAMAConnect: Driver not initialized in prepareToPlay. Audio will not stream.");
    }
}

void LAMAConnectACXAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    oscillator.reset();
    gain.reset();
    stopDriverAudio(); // This calls driverInterface.stopAudio()
    
    // Optionally, could call driverInterface.Shutdown() here if plugin is being fully unloaded
    // but typically Shutdown is for the destructor. stopAudio is usually enough for releaseResources.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LAMAConnectACXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This checks if the input layout matches the output layout
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // This checks if the layout is stereo or mono (adjust as needed)
    // For this plugin, we want to be flexible, up to 16 channels potentially (matching driver)
    // The actual channel count used with the driver is determined by 'channelLayoutParam'
    int numChannels = layouts.getMainOutputChannelSet().size();
    return (numChannels > 0 && numChannels <= MAX_AUDIO_CHANNELS); // MAX_AUDIO_CHANNELS is 16
  #endif
}
#endif

void LAMAConnectACXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear(); // We don't process MIDI

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Check for dynamic changes that require restarting audio stream
    // This is a simplified check. More robust would be to use parameter listeners.
    bool restartNeeded = false;
    if (currentSampleRate != getSampleRate() || currentBufferSize != buffer.getNumSamples()) {
        restartNeeded = true;
    }
    int layoutChannelCount = getChannelCountForLayout();
    if (layoutChannelCount != currentChannelCount && processingActive) { // only if active, otherwise prepareToPlay handles it
         restartNeeded = true;
    }


    if (restartNeeded && driverInitialized) {
        juce::Logger::writeToLog("LAMAConnect: Detected audio format change. Restarting driver audio.");
        stopDriverAudio();
        // Update current settings before starting again
        currentSampleRate = getSampleRate();
        currentBufferSize = buffer.getNumSamples();
        // currentChannelCount will be updated by startDriverAudio
        if (!startDriverAudio(currentSampleRate, currentBufferSize, layoutChannelCount)) {
             lastErrorMessage = driverInterface.GetLastErrorMsg();
             juce::Logger::writeToLog("LAMAConnect: Failed to restart driver audio in processBlock: " + lastErrorMessage);
        }
    }
    
    // Test Tone Generation (if enabled)
    if (testToneEnabled && testToneParam->get()) {
        oscillator.setFrequency(testToneFreqParam->get());
        gain.setGainDecibels(testToneGainParam->get());

        tempBuffer.setSize(totalNumOutputChannels, buffer.getNumSamples(), false, false, true);
        tempBuffer.clear(); // Clear before filling

        juce::dsp::AudioBlock<float> block (tempBuffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        oscillator.process(context);
        gain.process(context);

        // Mix test tone with input, or replace if no input
        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
            // If there's input, add. Otherwise, copy.
            if (channel < totalNumInputChannels) {
                 buffer.addFrom(channel, 0, tempBuffer, channel, 0, buffer.getNumSamples(), 1.0f);
            } else {
                 buffer.copyFrom(channel, 0, tempBuffer, channel, 0, buffer.getNumSamples());
            }
        }
    }


    // --- Driver Interaction ---
    if (driverInitialized && processingActive && driverInterface.IsActive())
    {
        // The number of channels JUCE is currently configured to use with the driver
        // This was set in startDriverAudio and is stored in currentChannelCount
        if (buffer.getNumChannels() < currentChannelCount) {
            // This case should ideally not happen if bus layouts are managed correctly.
            // If host provides fewer channels than we told the driver we'd use,
            // it's problematic. We'll silence and log.
            // juce::Logger::writeToLog("LAMAConnect: Host buffer has fewer channels than driver configuration. Silencing.");
            buffer.clear();
        } else {
            // We pass currentChannelCount as the number of channels to process with the driver.
            // driverInterface.processAudio will handle mapping these to/from the driver's 16 channels.
            if (!driverInterface.processAudio(
                    const_cast<float* const*>(buffer.getArrayOfReadPointers()), // JUCE input (to be sent to driver render)
                    buffer.getArrayOfWritePointers(),                           // JUCE output (filled from driver capture)
                    static_cast<UINT32>(currentChannelCount), // Use the channel count we configured the driver with
                    static_cast<UINT32>(buffer.getNumSamples())
            )) {
                lastErrorMessage = driverInterface.GetLastErrorMsg();
                // juce::Logger::writeToLog("LAMAConnect: driverInterface.processAudio failed: " + lastErrorMessage);
                // Consider stopping audio or just silencing this block
                buffer.clear(); 
            }
        }
    } else {
        // If driver not active or not initialized, pass through or silence
        // For a loopback/utility plugin, often we want silence if driver isn't working.
        // If test tone is the only thing active, it would have already been processed.
        if (!(testToneEnabled && testToneParam->get())) { // Don't clear if test tone just ran
             buffer.clear();
        }
    }

    // Update level meters (using plugin's actual output buffer)
    updateLevelMeters(buffer); 
}

//==============================================================================
bool LAMAConnectACXAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* LAMAConnectACXAudioProcessor::createEditor()
{
    return new LAMAConnectACXAudioProcessorEditor (*this);
}

//==============================================================================
void LAMAConnectACXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ValueTree state("LAMAConnectState");
    state.setProperty("testToneEnabled", var(testToneParam->get()), nullptr);
    state.setProperty("testToneFreq", var(testToneFreqParam->get()), nullptr);
    state.setProperty("testToneGain", var(testToneGainParam->get()), nullptr);
    state.setProperty("channelLayout", var(channelLayoutParam->getIndex()), nullptr);
    state.setProperty("driverInstance", var(selectedDriverIndex), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LAMAConnectACXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName("LAMAConnectState")) {
            testToneParam->setValueNotifyingHost(xmlState->getBoolAttribute("testTone", false));
            testToneFreqParam->setValueNotifyingHost(xmlState->getDoubleAttribute("testToneFreq", 440.0));
            testToneGainParam->setValueNotifyingHost(xmlState->getDoubleAttribute("testToneGain", -20.0));
            
            int layoutIndex = xmlState->getIntAttribute("channelLayout", 0);
            channelLayoutParam->setValueNotifyingHost(layoutIndex);
            
            int driverIdx = xmlState->getIntAttribute("driverInstance", 0);
            if (driverIdx != selectedDriverIndex) {
                 setDriverInstanceIndex(driverIdx); // This will handle re-init and audio restart if needed
            }
        }
    }
    // Update DSP from loaded parameters
    oscillator.setFrequency(testToneFreqParam->get());
    gain.setGainDecibels(testToneGainParam->get());
    testToneEnabled = testToneParam->get();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LAMAConnectACXAudioProcessor();
}

//==============================================================================
// Custom methods

bool LAMAConnectACXAudioProcessor::initializeDriver() {
    if (driverInitialized && driverInterface.IsInitialized() && 
        selectedDriverIndex == driverInterface.m_DeviceIndex) { // Assuming m_DeviceIndex from new header
        return true;
    }
    // If previously initialized, ensure it's shut down before re-initializing (especially for a different index)
    if(driverInitialized) {
        driverInterface.Shutdown();
        driverInitialized = false;
    }

    driverInitialized = driverInterface.Initialize(selectedDriverIndex);
    if (!driverInitialized) {
        lastErrorMessage = driverInterface.GetLastErrorMsg();
        juce::Logger::writeToLog("LAMAConnect: driverInterface.Initialize failed for index " + juce::String(selectedDriverIndex) + ": " + lastErrorMessage);
    } else {
        lastErrorMessage.clear();
        juce::Logger::writeToLog("LAMAConnect: driverInterface.Initialize successful for index " + juce::String(selectedDriverIndex));
    }
    return driverInitialized;
}

bool LAMAConnectACXAudioProcessor::startDriverAudio(int sampleRate, int bufferSize, int channelCount) {
    if (!driverInitialized) {
        lastErrorMessage = "Driver not initialized, cannot start audio.";
        juce::Logger::writeToLog("LAMAConnect: " + lastErrorMessage);
        processingActive = false;
        return false;
    }
    // If already active with the same settings, do nothing
    if (processingActive && this->currentSampleRate == sampleRate && 
        this->currentBufferSize == bufferSize && this->currentChannelCount == channelCount) {
        return true;
    }
    
    if (processingActive) { // Stop if active with different settings
        driverInterface.stopAudio();
        processingActive = false;
    }

    if (driverInterface.startAudio(static_cast<UINT32>(sampleRate), static_cast<UINT32>(bufferSize), static_cast<UINT32>(channelCount))) {
        processingActive = true;
        this->currentSampleRate = sampleRate;
        this->currentBufferSize = bufferSize;
        this->currentChannelCount = channelCount; // This is the plugin's view of active channels for the driver
        lastErrorMessage.clear();
        juce::Logger::writeToLog("LAMAConnect: Driver audio started. Rate: " + juce::String(sampleRate) +
                                 ", Buffer: " + juce::String(bufferSize) +
                                 ", Plugin Channels for Driver: " + juce::String(channelCount));
        return true;
    } else {
        processingActive = false;
        lastErrorMessage = driverInterface.GetLastErrorMsg();
         juce::Logger::writeToLog("LAMAConnect: driverInterface.startAudio failed: " + lastErrorMessage);
        return false;
    }
}

void LAMAConnectACXAudioProcessor::stopDriverAudio() {
    if (driverInitialized && (processingActive || driverInterface.IsActive())) { 
        driverInterface.stopAudio();
        processingActive = false;
        juce::Logger::writeToLog("LAMAConnect: Driver audio stopped.");
    }
    processingActive = false; // Ensure this is always set
}

bool LAMAConnectACXAudioProcessor::setDriverInstanceIndex(int index) {
    if (index < 0 || index > 3) { // Assuming max 4 instances (0-3)
        lastErrorMessage = "Invalid driver instance index: " + juce::String(index);
        return false;
    }

    if (selectedDriverIndex == index && driverInitialized && driverInterface.IsInitialized() &&
        driverInterface.m_DeviceIndex == index) { // Check against actual device index in interface
        return true; // No change needed
    }
    
    juce::Logger::writeToLog("LAMAConnect: Setting driver instance to " + juce::String(index));

    // Stop current streaming
    bool wasActive = processingActive;
    if (wasActive) {
        stopDriverAudio();
    }
    
    // Shutdown current interface before switching (SetDeviceIndex in interface handles this)
    // driverInterface.Shutdown(); // This is handled by driverInterface.SetDeviceIndex()
    // driverInitialized = false; // SetDeviceIndex will re-initialize

    selectedDriverIndex = index;
    
    // SetDeviceIndex in CLAMAConnectInterface will call Shutdown and then Initialize.
    if (!driverInterface.SetDeviceIndex(index)) {
        driverInitialized = false; // Ensure this is false if SetDeviceIndex fails
        lastErrorMessage = driverInterface.GetLastErrorMsg();
        juce::Logger::writeToLog("LAMAConnect: driverInterface.SetDeviceIndex failed for index " + juce::String(index) + ": " + lastErrorMessage);
        return false;
    }
    driverInitialized = true; // If SetDeviceIndex succeeded, it's initialized.
    lastErrorMessage.clear();
    

    // If audio was previously active, try to restart it with the new driver instance
    if (wasActive && currentSampleRate > 0 && currentBufferSize > 0 && currentChannelCount > 0) {
         juce::Logger::writeToLog("LAMAConnect: Attempting to restart audio on new driver instance " + juce::String(index));
        if (!startDriverAudio(currentSampleRate, currentBufferSize, currentChannelCount)) {
            // Error logged by startDriverAudio
            // processingActive will be false.
            return false; // Indicate that although index set, audio didn't restart
        }
    }
    juce::Logger::writeToLog("LAMAConnect: Successfully switched to driver instance " + juce::String(index));
    return true;
}

void LAMAConnectACXAudioProcessor::setTestToneFrequency(float freq) {
    if (testToneFreqParam)
        testToneFreqParam->setValueNotifyingHost(testToneFreqParam->getNormalisableRange().convertTo0to1(freq)); 
    oscillator.setFrequency(freq);
}

void LAMAConnectACXAudioProcessor::setTestToneGain(float gainLevelDB) {
    if (testToneGainParam)
        testToneGainParam->setValueNotifyingHost(testToneGainParam->getNormalisableRange().convertTo0to1(gainLevelDB)); 
    gain.setGainDecibels(gainLevelDB);
}

void LAMAConnectACXAudioProcessor::enableTestTone(bool enable) {
    if (testToneParam)
        testToneParam->setValueNotifyingHost(enable);
    testToneEnabled = enable;
}

// Helper to get channel count based on channelLayoutParam
int LAMAConnectACXAudioProcessor::getChannelCountForLayout() const {
    if (!channelLayoutParam) return 2; // Default to stereo if param is null

    int layoutIndex = channelLayoutParam->getIndex();
    // This mapping must match the 'channelChoices' in the constructor
    switch (layoutIndex) {
        case 0: return 2;  // Stereo (1-2)
        case 1: return 8;  // 8 Channels (1-8)
        case 2: return 16; // 16 Channels (1-16)
        // Add more cases if more layouts are added
        default: return 2;
    }
}

void LAMAConnectACXAudioProcessor::updateLevelMeters(const juce::AudioBuffer<float>& buffer) {
    // This is a very basic RMS calculation for demonstration.
    // A proper implementation would use smoothing, decibel conversion, etc.
    // For simplicity, just find peak.
    // inputLevels and outputLevels should be sized correctly in prepareToPlay.

    int numChannelsToMeter = buffer.getNumChannels(); // Meter what's actually in the buffer

    for (int ch = 0; ch < MAX_AUDIO_CHANNELS; ++ch) {
        float rmsIn = 0.0f;
        float rmsOut = 0.0f;
        
        if (ch < numChannelsToMeter) { // Only process channels that exist in the current host buffer
            // tempBuffer holds the input *before* test tone or driver processing
            if (ch < tempBuffer.getNumChannels() && buffer.getNumSamples() <= tempBuffer.getNumSamples()) {
                 rmsIn = tempBuffer.getRMSLevel(ch, 0, buffer.getNumSamples());
            }
            // buffer holds the final output after all processing
            rmsOut = buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
        }
        
        // Store levels (assuming inputLevels and outputLevels are for display and sized to MAX_AUDIO_CHANNELS)
        if (ch < inputLevels.getNumChannels())
            inputLevels.setSample(ch, 0, rmsIn);
        if (ch < outputLevels.getNumChannels())
            outputLevels.setSample(ch, 0, rmsOut);
    }
}

```
