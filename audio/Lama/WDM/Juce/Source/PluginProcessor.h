#pragma once
#include <JuceHeader.h>
#include "LAMAConnectInterface.h"

//==============================================================================
/**
 * LAMAConnectACXAudioProcessor
 * Routes audio between the DAW and the ACX virtual audio device.
 * The driver always uses 16 channels internally, but the plugin can use fewer.
 */
class LAMAConnectACXAudioProcessor : public juce::AudioProcessor {
public:
    LAMAConnectACXAudioProcessor();
    ~LAMAConnectACXAudioProcessor() override;

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

    // Driver control methods
    bool initializeDriver();
    bool startDriverAudio(int sampleRate, int bufferSize, int channelCount);
    void stopDriverAudio();
    juce::String getLastErrorMessage() const { return lastErrorMessage; }

    // Driver instance selection (0-3 for multiple virtual devices)
    bool setDriverInstanceIndex(int index);
    int getDriverInstanceIndex() const { return selectedDriverIndex; }

    // Test tone control
    void setTestToneFrequency(float freq);
    void setTestToneGain(float gainLevel);
    void enableTestTone(bool enable);

    // Level meter data access
    const juce::AudioBuffer<float>& getInputLevels() const { return inputLevels; }
    const juce::AudioBuffer<float>& getOutputLevels() const { return outputLevels; }

    // Parameter accessors
    bool getTestToneEnabled() const { return testToneEnabled; }
    float getTestToneFrequency() const { return oscillator.getFrequency(); }
    float getTestToneGain() const { return gain.getGainLinear(); }
    int getChannelCountForLayout() const;

    // Parameter getters for editor attachments
    juce::AudioParameterBool* getTestToneParam() const { return testToneParam; }
    juce::AudioParameterFloat* getTestToneFreqParam() const { return testToneFreqParam; }
    juce::AudioParameterFloat* getTestToneGainParam() const { return testToneGainParam; }
    juce::AudioParameterChoice* getChannelLayoutParam() const { return channelLayoutParam; }

    // Status accessors
    bool isDriverInitialized() const { return driverInitialized; }
    bool isProcessingActive() const { return processingActive; }

    // Channel layout helper for UI
    int getChannelLayoutIndex() const { return channelLayoutParam ? channelLayoutParam->getIndex() : 0; }

private:
    // Maximum channels supported (matches driver)
    static const int MAX_AUDIO_CHANNELS = 16;

    // Parameters
    juce::AudioParameterBool* testToneParam = nullptr;
    juce::AudioParameterFloat* testToneFreqParam = nullptr;
    juce::AudioParameterFloat* testToneGainParam = nullptr;
    juce::AudioParameterChoice* channelLayoutParam = nullptr;

    // Test tone DSP
    juce::dsp::Oscillator<float> oscillator;
    juce::dsp::Gain<float> gain;
    bool testToneEnabled = false;

    // Level meters
    juce::AudioBuffer<float> inputLevels;
    juce::AudioBuffer<float> outputLevels;
    juce::AudioBuffer<float> tempBuffer;
    void updateLevelMeters(const juce::AudioBuffer<float>& buffer);

    // Driver interface
    CLAMAConnectInterface driverInterface;
    bool driverInitialized = false;
    bool processingActive = false;
    juce::String lastErrorMessage;
    int selectedDriverIndex = 0;

    // Current format tracking
    int currentSampleRate = 0;
    int currentBufferSize = 0;
    int currentChannelCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LAMAConnectACXAudioProcessor)
};