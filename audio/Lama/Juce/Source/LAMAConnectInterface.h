#pragma once
#include <Windows.h>
#include <string>
#include "LAMAConnectShared.h"

// Interface class for communicating with the LAMAConnect driver
// The driver always uses 16 channels internally, but the plugin can use fewer
class CLAMAConnectInterface {
public:
    CLAMAConnectInterface();
    ~CLAMAConnectInterface();

    // Initialize connection to specific driver instance
    bool Initialize(int deviceIndex = 0);
    
    // Shutdown connection
    void Shutdown();

    // Switch to different driver instance
    bool SetDeviceIndex(int index);

    // Start audio streaming with specified format
    // Note: Driver always uses 16 channels internally, channelCount is the plugin's active count
    bool startAudio(UINT32 sampleRate, UINT32 framesPerBuffer, UINT32 channelCount);
    
    // Stop audio streaming
    void stopAudio();

    // Process one audio block
    // inputChannels/outputChannels arrays should have numChannels elements
    // Driver will handle conversion to/from its internal 16-channel format
    bool processAudio(float* const inputChannels[], float* const outputChannels[],
                      UINT32 numChannels, UINT32 numFrames);

    // Direct access to shared memory buffers (advanced use)
    float* GetPluginToDriverBuffer();
    float* GetDriverToPluginBuffer();

    // Status checks
    bool IsInitialized() const { return m_isInitialized; }
    bool IsActive() const;
    
    // Get the plugin's active channel count (not the driver's fixed 16)
    UINT32 GetActiveChannelCount() const { return m_activeChannelCount; }

private:
    std::wstring m_devicePath;
    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
    HANDLE m_hSharedMemory = NULL;
    HANDLE m_hCompletionEvent = NULL;
    PLAMA_CONNECT_SHARED_BUFFER m_pSharedBuffer = nullptr;
    void* m_pSharedMemoryBase = nullptr;
    bool m_isInitialized = false;
    UINT32 m_activeChannelCount = 2; // Plugin's active channel count (default stereo)
};