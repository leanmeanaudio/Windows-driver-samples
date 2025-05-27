#pragma once
#include <JuceHeader.h>
#include "LAMAConnectInterface.h"

//==============================================================================
/**
 * LAMAConnectACXAudioProcessor
 * Routes audio between the DAW and the ACX virtual audio device.
 * 
 * The driver always uses 16 channels internally for maximum compatibility,
 * but the plugin can be configured to use fewer channels (2-16).
 * The driver handles conversion between the plugin's active channel count
 * and its internal 16-channel format.
 * 
 * Features:
 * - Support for 2-16 channels (stereo up to full 16-channel)
 * - Multiple driver instances (0-3) for independent virtual devices
 * - Built-in test tone generator for testing
 * - Real-time level metering for all channels
 * - Automatic reconnection on driver communication failures
 * - Format change handling (sample rate, buffer size, channel count)
 */
class LAMAConnectACXAudioProcessor : public juce::AudioProcessor {
public:
    LAMAConnectACXAudioProcessor();
    ~LAMAConnectACXAudioProcessor() override;

    //==============================================================================
    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Driver control methods
    
    /**
     * Initialize connection to the driver
     * @return true if successful
     */
    bool initializeDriver();
    
    /**
     * Start audio streaming with specified format
     * @param sampleRate Sample rate in Hz
     * @param bufferSize Buffer size in frames
     * @param channelCount Number of active channels (2-16)
     * @return true if successful
     */
    bool startDriverAudio(int sampleRate, int bufferSize, int channelCount);
    
    /**
     * Stop audio streaming
     */
    void stopDriverAudio();
    
    /**
     * Get last error message from driver operations
     * @return Error description string
     */
    juce::String getLastErrorMessage() const { return lastErrorMessage; }

    //==============================================================================
    // Driver instance selection (0-3 for multiple virtual devices)
    
    /**
     * Switch to different driver instance
     * @param index Driver instance index (0-3)
     * @return true if successful
     */
    bool setDriverInstanceIndex(int index);
    
    /**
     * Get current driver instance index
     * @return Current instance index (0-3)
     */
    int getDriverInstanceIndex() const { return selectedDriverIndex; }

    //==============================================================================
    // Test tone control
    
    /**
     * Set test tone frequency
     * @param freq Frequency in Hz (20-20000)
     */
    void setTestToneFrequency(float freq);
    
    /**
     * Set test tone gain level
     * @param gainLevel Linear gain (0.0-1.0)
     */
    void setTestToneGain(float gainLevel);
    
    /**
     * Enable or disable test tone
     * @param enable true to enable, false to disable
     */
    void enableTestTone(bool enable);

    //==============================================================================
    // Level meter data access
    
    /**
     * Get current input levels for all channels
     * @return AudioBuffer containing RMS levels for each channel
     */
    const juce::AudioBuffer<float>& getInputLevels() const { return inputLevels; }
    
    /**
     * Get current output levels for all channels
     * @return AudioBuffer containing RMS levels for each channel
     */
    const juce::AudioBuffer<float>& getOutputLevels() const { return outputLevels; }

    //==============================================================================
    // Parameter accessors for UI
    
    bool getTestToneEnabled() const { return testToneEnabled; }
    float getTestToneFrequency() const { return oscillator.getFrequency(); }
    float getTestToneGain() const { return gain.getGainLinear(); }
    
    /**
     * Get number of channels for current layout selection
     * @return Channel count (2-16)
     */
    int getChannelCountForLayout() const;

    // Parameter getters for editor attachments
    juce::AudioParameterBool* getTestToneParam() const { return testToneParam; }
    juce::AudioParameterFloat* getTestToneFreqParam() const { return testToneFreqParam; }
    juce::AudioParameterFloat* getTestToneGainParam() const { return testToneGainParam; }
    juce::AudioParameterChoice* getChannelLayoutParam() const { return channelLayoutParam; }

    //==============================================================================
    // Status accessors
    
    /**
     * Check if driver interface is initialized
     * @return true if driver connection is established
     */
    bool isDriverInitialized() const { return driverInitialized; }
    
    /**
     * Check if audio processing is currently active
     * @return true if audio is being processed through driver
     */
    bool isProcessingActive() const { return processingActive; }

    // Channel layout helper for UI
    int getChannelLayoutIndex() const { return channelLayoutParam ? channelLayoutParam->getIndex() : 0; }

    //==============================================================================
    // Direct driver interface access (for advanced use)
    
    /**
     * Get direct access to driver interface
     * @return Reference to the driver interface object
     */
    CLAMAConnectInterface& getDriverInterface() { return driverInterface; }
    const CLAMAConnectInterface& getDriverInterface() const { return driverInterface; }

private:
    //==============================================================================
    // Constants
    
    // Maximum channels supported (matches driver capability)
    static const int MAX_AUDIO_CHANNELS = 16;

    //==============================================================================
    // Parameters
    
    juce::AudioParameterBool* testToneParam = nullptr;
    juce::AudioParameterFloat* testToneFreqParam = nullptr;
    juce::AudioParameterFloat* testToneGainParam = nullptr;
    juce::AudioParameterChoice* channelLayoutParam = nullptr;

    //==============================================================================
    // Test tone DSP
    
    juce::dsp::Oscillator<float> oscillator;
    juce::dsp::Gain<float> gain;
    bool testToneEnabled = false;

    //==============================================================================
    // Level meters
    
    juce::AudioBuffer<float> inputLevels;
    juce::AudioBuffer<float> outputLevels;
    juce::AudioBuffer<float> tempBuffer; // For storing input before processing
    
    /**
     * Update level meters with current audio buffer
     * @param buffer Audio buffer to analyze
     */
    void updateLevelMeters(const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // Driver interface
    
    CLAMAConnectInterface driverInterface;
    bool driverInitialized = false;
    bool processingActive = false;
    juce::String lastErrorMessage;
    int selectedDriverIndex = 0;

    //==============================================================================
    // Current format tracking
    
    int currentSampleRate = 0;
    int currentBufferSize = 0;
    int currentChannelCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LAMAConnectACXAudioProcessor)
};