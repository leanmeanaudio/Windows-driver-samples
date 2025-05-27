#include "PluginProcessor.h"
#include "PluginEditor.h"

LAMAConnectACXAudioProcessor::LAMAConnectACXAudioProcessor()
    : AudioProcessor(BusesProperties()
                    .withInput("Input", juce::AudioChannelSet::stereo(), true)    // Enable stereo by default
                    .withOutput("Output", juce::AudioChannelSet::stereo(), true)) // Enable stereo by default
{
    // Create parameters
    addParameter(testToneParam = new juce::AudioParameterBool("testTone", "Test Tone", false));
    addParameter(testToneFreqParam = new juce::AudioParameterFloat("toneFreq", "Tone Frequency", 
                                            20.0f, 20000.0f, 440.0f));
    addParameter(testToneGainParam = new juce::AudioParameterFloat("toneGain", "Tone Gain", 
                                            0.0f, 1.0f, 0.5f));
    
    // Channel layout choices - these determine how many of the driver's 16 channels we use
    const juce::StringArray layoutChoices = { 
        "Stereo (2)", "Quad (4)", "5.1 Surround (6)", "7.1 Surround (8)",
        "7.1.2 Atmos (10)", "7.1.4 Atmos (12)", "14-Channel (14)", "16-Channel (16)" 
    };
    addParameter(channelLayoutParam = new juce::AudioParameterChoice("layout", "Channel Layout", layoutChoices, 0));

    // Initialize DSP components
    oscillator.initialise([](float x){ return std::sin(x); });
    oscillator.setFrequency(*testToneFreqParam);
    gain.setGainLinear(*testToneGainParam);
    testToneEnabled = static_cast<bool>(*testToneParam);

    // Prepare level meter buffers
    inputLevels.setSize(MAX_AUDIO_CHANNELS, 1);
    outputLevels.setSize(MAX_AUDIO_CHANNELS, 1);
    tempBuffer.setSize(MAX_AUDIO_CHANNELS, 0);

    // Try to initialize driver connection
    if (!driverInitialized) {
        driverInitialized = driverInterface.Initialize(selectedDriverIndex);
        if (!driverInitialized) {
            lastErrorMessage = "Unable to open LAMAConnect device - check if driver is installed and running";
            DBG("Driver initialization failed: " + lastErrorMessage);
        } else {
            DBG("Driver interface initialized successfully");
        }
    }
}

LAMAConnectACXAudioProcessor::~LAMAConnectACXAudioProcessor() {
    DBG("LAMAConnectACXAudioProcessor destructor");
    stopDriverAudio();
    driverInterface.Shutdown();
}

const juce::String LAMAConnectACXAudioProcessor::getName() const { return "LAMAConnectACX"; }
bool LAMAConnectACXAudioProcessor::acceptsMidi() const { return false; }
bool LAMAConnectACXAudioProcessor::producesMidi() const { return false; }
bool LAMAConnectACXAudioProcessor::isMidiEffect() const { return false; }
double LAMAConnectACXAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int LAMAConnectACXAudioProcessor::getNumPrograms() { return 1; }
int LAMAConnectACXAudioProcessor::getCurrentProgram() { return 0; }
void LAMAConnectACXAudioProcessor::setCurrentProgram(int) {}
const juce::String LAMAConnectACXAudioProcessor::getProgramName(int) { return {}; }
void LAMAConnectACXAudioProcessor::changeProgramName(int, const juce::String&) {}

void LAMAConnectACXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    DBG("prepareToPlay: " + juce::String(sampleRate) + " Hz, " + juce::String(samplesPerBlock) + " samples");
    
    // Get actual channel count from host (not from parameter)
    int actualChannels = std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    actualChannels = std::max(actualChannels, 2); // Minimum stereo
    
    DBG("Host provides " + juce::String(actualChannels) + " channels");
    
    // Initialize DSP components with actual channel count
    juce::dsp::ProcessSpec spec { 
        sampleRate, 
        static_cast<juce::uint32>(samplesPerBlock), 
        static_cast<juce::uint32>(actualChannels) 
    };
    oscillator.prepare(spec);
    gain.prepare(spec);

    // Initialize driver if needed
    if (!driverInitialized) {
        driverInitialized = driverInterface.Initialize(selectedDriverIndex);
        if (!driverInitialized) {
            lastErrorMessage = "Driver not initialized - check if LAMAConnect driver is installed and running";
            DBG("Driver initialization failed in prepareToPlay");
            return;
        }
    }
    
    // Start driver audio with actual channel count from host
    if (driverInitialized) {
        if (startDriverAudio((int)sampleRate, samplesPerBlock, actualChannels)) {
            currentSampleRate = (int)sampleRate;
            currentBufferSize = samplesPerBlock;
            currentChannelCount = actualChannels;
            DBG("Driver audio started successfully: " + juce::String(sampleRate) + " Hz, " + 
                juce::String(samplesPerBlock) + " samples, " + juce::String(actualChannels) + " channels");
        } else {
            DBG("Failed to start driver audio: " + lastErrorMessage);
        }
    }
    
    // Prepare temp buffer for level metering
    tempBuffer.setSize(actualChannels, samplesPerBlock, false, false, true);
}

void LAMAConnectACXAudioProcessor::releaseResources() {
    DBG("releaseResources called");
    stopDriverAudio();
}

bool LAMAConnectACXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& inputSet  = layouts.getMainInputChannelSet();
    const auto& outputSet = layouts.getMainOutputChannelSet();
    
    // Must have matching input/output channel counts
    if (inputSet.size() != outputSet.size()) {
        return false;
    }
    
    int channelCount = inputSet.size();
    
    // Support any channel count from 1 to 16, but prefer standard layouts
    if (channelCount < 1 || channelCount > 16) {
        return false;
    }
    
    // For standard channel counts, prefer proper channel sets
    switch (channelCount) {
        case 1:  return inputSet == juce::AudioChannelSet::mono() && 
                        outputSet == juce::AudioChannelSet::mono();
        case 2:  return inputSet == juce::AudioChannelSet::stereo() && 
                        outputSet == juce::AudioChannelSet::stereo();
        case 4:  return inputSet == juce::AudioChannelSet::quadraphonic() && 
                        outputSet == juce::AudioChannelSet::quadraphonic();
        case 6:  return inputSet == juce::AudioChannelSet::create5point1() && 
                        outputSet == juce::AudioChannelSet::create5point1();
        case 8:  return inputSet == juce::AudioChannelSet::create7point1() && 
                        outputSet == juce::AudioChannelSet::create7point1();
        case 10: return inputSet == juce::AudioChannelSet::create7point1point2() && 
                        outputSet == juce::AudioChannelSet::create7point1point2();
        case 12: return inputSet == juce::AudioChannelSet::create7point1point4() && 
                        outputSet == juce::AudioChannelSet::create7point1point4();
        case 16: return true; // Accept any 16-channel configuration
        default: 
            // For other counts (3,5,7,9,11,13,14,15), accept discrete channel sets
            return inputSet.isDiscreteLayout() && outputSet.isDiscreteLayout();
    }
}

void LAMAConnectACXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Validate buffer
    if (numChannels <= 0 || numSamples <= 0) {
        return;
    }

    // Update test tone parameters if changed
    if (testToneEnabled != (bool)*testToneParam) {
        enableTestTone((bool)*testToneParam);
    }
    if (std::abs(oscillator.getFrequency() - *testToneFreqParam) > 0.1f) {
        setTestToneFrequency(*testToneFreqParam);
    }
    if (std::abs(gain.getGainLinear() - *testToneGainParam) > 0.001f) {
        setTestToneGain(*testToneGainParam);
    }

    // Check if host format changed
    int hostRate = (int)getSampleRate();
    int hostBlock = buffer.getNumSamples();
    int hostChannels = numChannels; // Use actual buffer channels
    
    if (hostRate != currentSampleRate || hostBlock != currentBufferSize || hostChannels != currentChannelCount) {
        DBG("Format change detected: " + juce::String(hostRate) + " Hz, " + 
            juce::String(hostBlock) + " samples, " + juce::String(hostChannels) + " channels");
        
        if (processingActive) {
            stopDriverAudio();
        }
        
        if (!driverInitialized) {
            driverInitialized = driverInterface.Initialize(selectedDriverIndex);
        }
        
        if (driverInitialized && startDriverAudio(hostRate, hostBlock, hostChannels)) {
            currentSampleRate = hostRate;
            currentBufferSize = hostBlock;
            currentChannelCount = hostChannels;
        }
    }

    // Generate test tone if enabled (before saving input for meters)
    if (testToneEnabled) {
        // Ensure oscillator is set up for correct channel count
        juce::dsp::AudioBlock<float> audioBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);
        oscillator.process(context);
        gain.process(context);
    }

    // Save input for level meters (after test tone if enabled)
    tempBuffer.setSize(numChannels, numSamples, false, false, true);
    tempBuffer.makeCopyOf(buffer);

    // Process through driver if active
    if (processingActive && driverInterface.IsActive()) {
        static int failureCount = 0;
        
        try {
            // Validate channel count
            if (numChannels > MAX_AUDIO_CHANNELS) {
                DBG("Warning: Channel count " + juce::String(numChannels) + " exceeds maximum " + juce::String(MAX_AUDIO_CHANNELS));
            }
            
            // Prepare channel pointer arrays
            float* inputPtrs[MAX_AUDIO_CHANNELS];
            float* outputPtrs[MAX_AUDIO_CHANNELS];
            
            int actualChannels = std::min(numChannels, MAX_AUDIO_CHANNELS);
            for (int ch = 0; ch < actualChannels; ++ch) {
                inputPtrs[ch] = buffer.getWritePointer(ch);
                outputPtrs[ch] = buffer.getWritePointer(ch);
            }
            
            // Process through driver interface
            if (!driverInterface.processAudio(inputPtrs, outputPtrs, (UINT32)actualChannels, (UINT32)numSamples)) {
                if (failureCount == 0) { // Only log first failure
                    DBG("Driver processing failed - attempt " + juce::String(failureCount + 1));
                }
                if (++failureCount > 3) { // Reduced threshold
                    failureCount = 0;
                    stopDriverAudio();
                    lastErrorMessage = "Driver communication lost - stopped processing";
                    DBG(lastErrorMessage);
                }
                // Keep test tone if enabled, otherwise clear
                if (!testToneEnabled) {
                    buffer.clear();
                }
            } else {
                failureCount = 0; // Reset failure count on success
            }
        } catch (const std::exception& e) {
            DBG("Exception in driver processing: " + juce::String(e.what()));
            stopDriverAudio();
            lastErrorMessage = "Processing exception: " + juce::String(e.what());
            if (!testToneEnabled) {
                buffer.clear();
            }
        }
    } else if (!testToneEnabled) {
        // Driver not active and no test tone - clear buffer
        buffer.clear();
    }

    // Update level meters
    updateLevelMeters(buffer);
}

bool LAMAConnectACXAudioProcessor::initializeDriver() {
    DBG("Attempting to initialize driver (instance " + juce::String(selectedDriverIndex) + ")");
    
    bool ok = driverInterface.Initialize(selectedDriverIndex);
    driverInitialized = ok;
    
    if (!ok) {
        lastErrorMessage = "Failed to initialize driver interface - check if LAMAConnect driver is installed";
        DBG("Driver initialization failed: " + lastErrorMessage);
    } else {
        lastErrorMessage.clear();
        DBG("Driver initialized successfully");
        
        // Print driver status for debugging
        driverInterface.PrintStatus();
    }
    
    return ok;
}

bool LAMAConnectACXAudioProcessor::startDriverAudio(int sampleRate, int bufferSize, int channelCount) {
    if (!driverInitialized) {
        if (!initializeDriver()) {
            return false;
        }
    }
    
    DBG("Starting driver audio: " + juce::String(sampleRate) + " Hz, " + 
        juce::String(bufferSize) + " frames, " + juce::String(channelCount) + " channels");
    
    // Start audio with the plugin's active channel count
    // The driver will always use 16 channels internally
    bool started = driverInterface.startAudio(sampleRate, bufferSize, channelCount);
    
    if (started) {
        processingActive = true;
        lastErrorMessage.clear();
        DBG("Driver audio started successfully");
    } else {
        processingActive = false;
        lastErrorMessage = "Driver start failed - check driver status and try different sample rate/buffer size";
        DBG("Driver audio start failed: " + lastErrorMessage);
    }
    
    return started;
}

void LAMAConnectACXAudioProcessor::stopDriverAudio() {
    if (processingActive || driverInterface.IsActive()) {
        driverInterface.stopAudio();
        DBG("Driver audio stopped");
    }
    processingActive = false;
}

bool LAMAConnectACXAudioProcessor::setDriverInstanceIndex(int index) {
    if (index == selectedDriverIndex) {
        return true;
    }
    
    DBG("Switching driver instance from " + juce::String(selectedDriverIndex) + " to " + juce::String(index));
    
    // Stop current streaming
    if (processingActive) {
        stopDriverAudio();
    }
    
    // Switch to new driver instance
    bool ok = driverInterface.SetDeviceIndex(index);
    
    if (ok) {
        selectedDriverIndex = index;
        driverInitialized = true;
        lastErrorMessage.clear();
        DBG("Successfully switched to driver instance " + juce::String(index));
    } else {
        driverInitialized = false;
        lastErrorMessage = "Failed to switch driver instance - check if instance " + juce::String(index) + " is available";
        DBG("Failed to switch driver instance: " + lastErrorMessage);
    }
    
    return ok;
}

void LAMAConnectACXAudioProcessor::setTestToneFrequency(float freq) {
    oscillator.setFrequency(freq);
}

void LAMAConnectACXAudioProcessor::setTestToneGain(float gainLevel) {
    gain.setGainLinear(gainLevel);
}

void LAMAConnectACXAudioProcessor::enableTestTone(bool enable) {
    testToneEnabled = enable;
    if (testToneParam != nullptr) {
        testToneParam->operator=(enable);
    }
    DBG("Test tone " + juce::String(enable ? "enabled" : "disabled"));
}

int LAMAConnectACXAudioProcessor::getChannelCountForLayout() const {
    int layoutIndex = channelLayoutParam ? channelLayoutParam->getIndex() : 0;
    
    switch (layoutIndex) {
        case 0:  return 2;   // Stereo
        case 1:  return 4;   // Quad
        case 2:  return 6;   // 5.1
        case 3:  return 8;   // 7.1
        case 4:  return 10;  // 7.1.2
        case 5:  return 12;  // 7.1.4
        case 6:  return 14;  // 14 channels
        case 7:  return 16;  // 16 channels (full driver capability)
        default: return 2;
    }
}

void LAMAConnectACXAudioProcessor::updateLevelMeters(const juce::AudioBuffer<float>& buffer) {
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int ch = 0; ch < MAX_AUDIO_CHANNELS; ++ch) {
        float rmsIn = 0.0f;
        float rmsOut = 0.0f;
        
        if (ch < numChannels) {
            // Compute RMS for input (pre-processed)
            if (ch < tempBuffer.getNumChannels()) {
                const float* inData = tempBuffer.getReadPointer(ch);
                double sumSq = 0.0;
                for (int i = 0; i < numSamples; ++i) {
                    float s = inData[i];
                    sumSq += s * s;
                }
                rmsIn = (float)std::sqrt(sumSq / (double)numSamples);
            }
            
            // Compute RMS for output (post-processed)
            const float* outData = buffer.getReadPointer(ch);
            double sumSqOut = 0.0;
            for (int i = 0; i < numSamples; ++i) {
                float s = outData[i];
                sumSqOut += s * s;
            }
            rmsOut = (float)std::sqrt(sumSqOut / (double)numSamples);
        }
        
        // Store levels with simple peak hold
        float currentInLevel = inputLevels.getSample(ch, 0);
        float currentOutLevel = outputLevels.getSample(ch, 0);
        
        // Simple peak decay
        currentInLevel = std::max(rmsIn, currentInLevel * 0.95f);
        currentOutLevel = std::max(rmsOut, currentOutLevel * 0.95f);
        
        inputLevels.setSample(ch, 0, currentInLevel);
        outputLevels.setSample(ch, 0, currentOutLevel);
    }
}

juce::AudioProcessorEditor* LAMAConnectACXAudioProcessor::createEditor() {
    return new LAMAConnectACXAudioProcessorEditor(*this);
}

bool LAMAConnectACXAudioProcessor::hasEditor() const {
    return true;
}

void LAMAConnectACXAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream out(destData, true);
    out.writeFloat(*testToneFreqParam);
    out.writeFloat(*testToneGainParam);
    out.writeBool(*testToneParam);
    out.writeInt(channelLayoutParam->getIndex());
    out.writeInt(selectedDriverIndex);
    
    DBG("State saved: freq=" + juce::String(*testToneFreqParam) + 
        ", gain=" + juce::String(*testToneGainParam) + 
        ", tone=" + juce::String(*testToneParam ? "on" : "off") + 
        ", layout=" + juce::String(channelLayoutParam->getIndex()) + 
        ", driver=" + juce::String(selectedDriverIndex));
}

void LAMAConnectACXAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream in(data, static_cast<size_t>(sizeInBytes), false);
    
    if (sizeInBytes >= (int)(sizeof(float)*2 + sizeof(bool) + sizeof(int)*2)) {
        float freq = in.readFloat();
        float gainVal = in.readFloat();
        bool toneEnabled = in.readBool();
        int layoutIdx = in.readInt();
        int driverIdx = in.readInt();
        
        testToneFreqParam->operator=(freq);
        testToneGainParam->operator=(gainVal);
        testToneParam->operator=(toneEnabled ? 1.0f : 0.0f);
        channelLayoutParam->operator=(layoutIdx);
        
        // Update internal state
        setTestToneFrequency(freq);
        setTestToneGain(gainVal);
        enableTestTone(toneEnabled);
        
        // Switch driver instance if different
        if (driverIdx != selectedDriverIndex) {
            setDriverInstanceIndex(driverIdx);
        }
        
        DBG("State restored: freq=" + juce::String(freq) + 
            ", gain=" + juce::String(gainVal) + 
            ", tone=" + juce::String(toneEnabled ? "on" : "off") + 
            ", layout=" + juce::String(layoutIdx) + 
            ", driver=" + juce::String(driverIdx));
    } else {
        DBG("State restore failed: insufficient data size " + juce::String(sizeInBytes));
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LAMAConnectACXAudioProcessor();
}