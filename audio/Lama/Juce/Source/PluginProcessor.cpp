#include "PluginProcessor.h"
#include "PluginEditor.h"

LAMAConnectACXAudioProcessor::LAMAConnectACXAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::disabled(), true)
                                    .withOutput("Output", juce::AudioChannelSet::disabled(), true))
{
    // Create parameters
    addParameter(testToneParam = new juce::AudioParameterBool("testTone", "Test Tone", false));
    addParameter(testToneFreqParam = new juce::AudioParameterFloat("toneFreq", "Tone Frequency", 
                                            20.0f, 20000.0f, 440.0f));
    addParameter(testToneGainParam = new juce::AudioParameterFloat("toneGain", "Tone Gain", 
                                            0.0f, 1.0f, 0.5f));
    
    // Channel layout choices - these determine how many of the driver's 16 channels we use
    const juce::StringArray layoutChoices = { 
        "Stereo (2)", "5.1 Surround (6)", "7.1 Surround (8)",
        "7.1.2 Atmos (10)", "7.1.4 Atmos (12)", "16-Channel (16)" 
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
            lastErrorMessage = "Unable to open LAMAConnect device";
        }
    }
}

LAMAConnectACXAudioProcessor::~LAMAConnectACXAudioProcessor() {
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
    // Initialize DSP components
    juce::dsp::ProcessSpec spec { 
        sampleRate, 
        static_cast<juce::uint32>(samplesPerBlock), 
        static_cast<juce::uint32>(getChannelCountForLayout()) 
    };
    oscillator.prepare(spec);
    gain.prepare(spec);

    // Initialize driver if needed
    if (!driverInitialized) {
        driverInitialized = driverInterface.Initialize(selectedDriverIndex);
        if (!driverInitialized) {
            lastErrorMessage = "Driver not initialized";
            return;
        }
    }
    
    // Start driver audio with current format
    if (driverInitialized) {
        int activeChannels = getChannelCountForLayout();
        if (startDriverAudio((int)sampleRate, samplesPerBlock, activeChannels)) {
            currentSampleRate = (int)sampleRate;
            currentBufferSize = samplesPerBlock;
            currentChannelCount = activeChannels;
        }
    }
}

void LAMAConnectACXAudioProcessor::releaseResources() {
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
    
    // Support common layouts that match our channel count options
    switch (channelCount) {
        case 2:  return true; // Stereo
        case 6:  return inputSet == juce::AudioChannelSet::create5point1() && 
                        outputSet == juce::AudioChannelSet::create5point1();
        case 8:  return inputSet == juce::AudioChannelSet::create7point1() && 
                        outputSet == juce::AudioChannelSet::create7point1();
        case 10: return inputSet == juce::AudioChannelSet::create7point1point2() && 
                        outputSet == juce::AudioChannelSet::create7point1point2();
        case 12: return inputSet == juce::AudioChannelSet::create7point1point4() && 
                        outputSet == juce::AudioChannelSet::create7point1point4();
        case 16: return true; // 16-channel configuration
        default: return false;
    }
}

void LAMAConnectACXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Update test tone parameters if changed
    if (testToneEnabled != (bool)*testToneParam) {
        enableTestTone((bool)*testToneParam);
    }
    if (oscillator.getFrequency() != *testToneFreqParam) {
        setTestToneFrequency(*testToneFreqParam);
    }
    if (gain.getGainLinear() != *testToneGainParam) {
        setTestToneGain(*testToneGainParam);
    }

    // Check if host format changed
    int hostRate = (int)getSampleRate();
    int hostBlock = buffer.getNumSamples();
    int hostChannels = getChannelCountForLayout();
    
    if (hostRate != currentSampleRate || hostBlock != currentBufferSize || hostChannels != currentChannelCount) {
        // Format changed - restart driver audio
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

    // Generate test tone if enabled
    if (testToneEnabled) {
        juce::dsp::AudioBlock<float> audioBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);
        oscillator.process(context);
        gain.process(context);
    }

    // Save input for level meters
    tempBuffer.setSize(numChannels, numSamples, false, false, true);
    tempBuffer.makeCopyOf(buffer);

    // Process through driver if active
    if (processingActive) {
        static int failureCount = 0;
        
        try {
            // Prepare channel pointer arrays
            float* inputPtrs[MAX_AUDIO_CHANNELS];
            float* outputPtrs[MAX_AUDIO_CHANNELS];
            
            for (int ch = 0; ch < numChannels && ch < MAX_AUDIO_CHANNELS; ++ch) {
                inputPtrs[ch] = buffer.getWritePointer(ch); // Use getWritePointer for mutable input
                outputPtrs[ch] = buffer.getWritePointer(ch);
            }
            
            // Process through driver interface
            // The driver will handle conversion to/from its internal 16-channel format
            if (!driverInterface.processAudio(inputPtrs, outputPtrs, (UINT32)numChannels, (UINT32)numSamples)) {
                DBG("Driver processing failed - resetting connection");
                if (++failureCount > 5) {
                    failureCount = 0;
                    stopDriverAudio();
                    // Don't auto-restart to avoid rapid flapping
                }
            } else {
                failureCount = 0;
            }
        } catch (const std::exception& e) {
            DBG("Exception in driver processing: " + juce::String(e.what()));
            // On exception, keep audio safe
            if (!testToneEnabled) {
                buffer.clear();
            }
        }
    }

    // Update level meters
    updateLevelMeters(buffer);
}

bool LAMAConnectACXAudioProcessor::initializeDriver() {
    bool ok = driverInterface.Initialize(selectedDriverIndex);
    driverInitialized = ok;
    
    if (!ok) {
        lastErrorMessage = "Failed to initialize driver interface";
    } else {
        lastErrorMessage.clear();
    }
    
    return ok;
}

bool LAMAConnectACXAudioProcessor::startDriverAudio(int sampleRate, int bufferSize, int channelCount) {
    if (!driverInitialized) {
        if (!initializeDriver()) {
            return false;
        }
    }
    
    // Start audio with the plugin's active channel count
    // The driver will always use 16 channels internally
    bool started = driverInterface.startAudio(sampleRate, bufferSize, channelCount);
    
    if (started) {
        processingActive = true;
        lastErrorMessage.clear();
        DBG("Driver audio started: " + juce::String(sampleRate) + " Hz, " + 
            juce::String(bufferSize) + " frames, " + juce::String(channelCount) + " channels");
    } else {
        processingActive = false;
        lastErrorMessage = "Driver start failed";
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
        DBG("Switched to driver instance " + juce::String(index));
    } else {
        driverInitialized = false;
        lastErrorMessage = "Failed to switch driver instance";
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
}

int LAMAConnectACXAudioProcessor::getChannelCountForLayout() const {
    int layoutIndex = channelLayoutParam ? channelLayoutParam->getIndex() : 0;
    
    switch (layoutIndex) {
        case 0:  return 2;   // Stereo
        case 1:  return 6;   // 5.1
        case 2:  return 8;   // 7.1
        case 3:  return 10;  // 7.1.2
        case 4:  return 12;  // 7.1.4
        case 5:  return 16;  // 16 channels (full driver capability)
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
        
        // Store levels
        inputLevels.setSample(ch, 0, rmsIn);
        outputLevels.setSample(ch, 0, rmsOut);
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
}

void LAMAConnectACXAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    juce::MemoryInputStream in(data, static_cast<size_t>(sizeInBytes), false);
    
    if (sizeInBytes >= (int)(sizeof(float)*2 + sizeof(bool) + sizeof(int)*2)) {
        testToneFreqParam->operator=(in.readFloat());
        testToneGainParam->operator=(in.readFloat());
        testToneParam->operator=(in.readBool() ? 1.0f : 0.0f);
        
        int layoutIdx = in.readInt();
        channelLayoutParam->operator=(layoutIdx);
        
        int driverIdx = in.readInt();
        setDriverInstanceIndex(driverIdx);
    }
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LAMAConnectACXAudioProcessor();
}