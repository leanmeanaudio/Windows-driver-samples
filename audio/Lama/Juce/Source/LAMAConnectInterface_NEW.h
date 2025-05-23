#pragma once
// #include <JuceHeader.h> // Included via PluginProcessor.h or other main JUCE headers normally
#include <Windows.h>
#include <string>
#include <vector>
#include <ks.h>       // For KSSTATE etc.
#include <ksmedia.h>  // For KSDATAFORMAT_WAVEFORMATEXTENSIBLE etc.

class CLAMAConnectInterface {
public:
    CLAMAConnectInterface();
    ~CLAMAConnectInterface();

    // Initializes connection to a specific driver instance (0-3)
    bool Initialize(int deviceIndex = 0);

    // Shuts down the connection to the driver
    void Shutdown();

    // Switches to a different driver instance
    bool SetDeviceIndex(int index);

    // Starts audio streaming with the driver
    // pluginChannelCount is the number of channels JUCE wants to use from its perspective
    bool startAudio(UINT32 sampleRate, UINT32 framesPerBuffer, UINT32 pluginChannelCount);

    // Stops audio streaming with the driver
    void stopAudio();

    // Processes one block of audio.
    // inputChannels: array of float pointers from JUCE's input buffer (data to send to driver's render endpoint)
    // outputChannels: array of float pointers for JUCE's output buffer (data received from driver's capture endpoint)
    // numPluginChannels: number of active channels JUCE is processing (e.g., buffer.getNumChannels())
    // numFrames: number of samples in this block (e.g., buffer.getNumSamples())
    bool processAudio(float* const inputChannels[], float* const outputChannels[],
                      UINT32 numPluginChannels, UINT32 numFrames);

    // Status checks
    bool IsInitialized() const { return m_isInitialized; }
    bool IsActive() const { return m_bIsActive; }

    // Gets the number of channels the plugin is actively using with the driver
    UINT32 GetActivePluginChannelCount() const { return m_PluginChannelCount; }

    // Gets the last error message
    std::wstring GetLastErrorMsg() const { return m_lastErrorMsg; }

private:
    // Helper to construct device path string (e.g., \\.\LamaLoopbackRender0)
    std::wstring GetDevicePath(int deviceIndex, bool isRender);

    // Handles to the render (write to driver) and capture (read from driver) endpoints
    HANDLE m_hRenderDevice = INVALID_HANDLE_VALUE;
    HANDLE m_hCaptureDevice = INVALID_HANDLE_VALUE;
    
    // Store last used device paths
    std::wstring m_DevicePathRender;
    std::wstring m_DevicePathCapture;

    // Current audio format parameters
    UINT32 m_SampleRate = 0;
    UINT32 m_FramesPerBuffer = 0;
    UINT32 m_PluginChannelCount = 0; // Number of channels JUCE is using

    // Driver's fixed audio format characteristics
    static const UINT32 m_DriverChannelCount = 16;    // Driver always operates with 16 channels
    static const UINT16 m_DriverBitsPerSample = 16; // Driver uses 16-bit samples

    // State flags
    bool m_isInitialized = false; // Is the interface initialized (device handles potentially open)
    bool m_bIsActive = false;     // Is audio streaming active (driver format set, pins in RUN state)
    int m_DeviceIndex = 0;        // Currently selected driver instance

    // Intermediate buffers for converting/interleaving data for driver I/O
    // These will hold 16-bit signed integer (S16LE) interleaved data for all 16 driver channels.
    std::vector<INT16> m_DriverIOBufferRender;
    std::vector<INT16> m_DriverIOBufferCapture;
   
    // Stores the last error message from API calls
    std::wstring m_lastErrorMsg;
};
