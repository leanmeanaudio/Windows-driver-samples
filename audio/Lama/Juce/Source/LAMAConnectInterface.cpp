#include "LAMAConnectInterface.h"
#include <stdio.h>

#define LAMA_CONNECT_DEVICE_BASE_NAME L"\\\\.\\LAMAConnect"

CLAMAConnectInterface::CLAMAConnectInterface() {
    // Member variables are default-initialized in the header
}

CLAMAConnectInterface::~CLAMAConnectInterface() {
    Shutdown();
}

bool CLAMAConnectInterface::Initialize(int deviceIndex) {
    if (m_isInitialized) {
        return true;
    }
    
    // Form device path
    wchar_t devPath[64];
    swprintf(devPath, 64, L"%s%d", LAMA_CONNECT_DEVICE_BASE_NAME, deviceIndex);
    m_devicePath = devPath;
    
    // Open driver device
    m_hDevice = CreateFileW(m_devicePath.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        DWORD errorCode = GetLastError();
        printf("Failed to open device %ls (error %lu)\n", m_devicePath.c_str(), errorCode);
        return false;
    }
    
    // Open shared memory
    wchar_t memName[64];
    swprintf(memName, 64, L"%s%d", LAMA_CONNECT_SHARED_MEMORY_NAME, deviceIndex);
    m_hSharedMemory = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, memName);
    if (m_hSharedMemory == NULL) {
        printf("Could not open file mapping '%ls' (error %lu)\n", memName, GetLastError());
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
        return false;
    }
    
    // Map shared memory
    m_pSharedMemoryBase = MapViewOfFile(m_hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (m_pSharedMemoryBase == NULL) {
        printf("Could not map shared memory (error %lu)\n", GetLastError());
        CloseHandle(m_hSharedMemory);
        CloseHandle(m_hDevice);
        m_hSharedMemory = NULL;
        m_hDevice = INVALID_HANDLE_VALUE;
        return false;
    }
    
    // Get shared buffer pointer
    m_pSharedBuffer = reinterpret_cast<PLAMA_CONNECT_SHARED_BUFFER>(m_pSharedMemoryBase);
    
    // Open completion event
    wchar_t evtName[64];
    swprintf(evtName, 64, L"%s%d", LAMA_CONNECT_COMPLETION_EVENT_NAME, deviceIndex);
    m_hCompletionEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, evtName);
    if (m_hCompletionEvent == NULL) {
        printf("OpenEvent failed for '%ls' (error %lu). Using polling fallback.\n",
               evtName, GetLastError());
        // Not fatal - will use polling
    }
    
    m_isInitialized = true;
    return true;
}

void CLAMAConnectInterface::Shutdown() {
    if (!m_isInitialized) return;
    
    if (m_pSharedMemoryBase) {
        UnmapViewOfFile(m_pSharedMemoryBase);
        m_pSharedMemoryBase = nullptr;
        m_pSharedBuffer = nullptr;
    }
    
    if (m_hSharedMemory) {
        CloseHandle(m_hSharedMemory);
        m_hSharedMemory = NULL;
    }
    
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
    
    if (m_hCompletionEvent) {
        CloseHandle(m_hCompletionEvent);
        m_hCompletionEvent = NULL;
    }
    
    m_isInitialized = false;
}

bool CLAMAConnectInterface::SetDeviceIndex(int index) {
    Shutdown();
    return Initialize(index);
}

bool CLAMAConnectInterface::startAudio(UINT32 sampleRate, UINT32 framesPerBuffer, UINT32 channelCount) {
    if (!Initialize()) {
        return false;
    }
    
    // Register with driver
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(m_hDevice,
                             IOCTL_LAMA_CONNECT_REGISTER,
                             NULL, 0,
                             NULL, 0,
                             &bytesReturned,
                             NULL);
    if (!ok) {
        printf("IOCTL_LAMA_CONNECT_REGISTER failed (error %lu)\n", GetLastError());
        return false;
    }
    
    // Set format - driver will always use 16 channels but we tell it our active count
    LAMA_CONNECT_FORMAT fmt;
    fmt.SampleRate = sampleRate;
    fmt.ChannelCount = channelCount; // Plugin's active channel count (driver ignores this)
    fmt.BufferSize = framesPerBuffer;
    
    ok = DeviceIoControl(m_hDevice,
                        IOCTL_LAMA_CONNECT_SET_FORMAT,
                        &fmt, sizeof(fmt),
                        NULL, 0,
                        &bytesReturned,
                        NULL);
    if (!ok) {
        printf("IOCTL_LAMA_CONNECT_SET_FORMAT failed (error %lu)\n", GetLastError());
        return false;
    }
    
    // Store the plugin's active channel count for our own use
    m_activeChannelCount = channelCount;
    
    if (m_pSharedBuffer) {
        m_pSharedBuffer->IsActive = TRUE;
    }
    
    return true;
}

void CLAMAConnectInterface::stopAudio() {
    if (!m_isInitialized) {
        return;
    }
    
    DWORD bytesReturned;
    DeviceIoControl(m_hDevice,
                   IOCTL_LAMA_CONNECT_UNREGISTER,
                   NULL, 0, NULL, 0,
                   &bytesReturned, NULL);
    
    if (m_pSharedBuffer) {
        m_pSharedBuffer->IsActive = FALSE;
    }
}

bool CLAMAConnectInterface::processAudio(float* const inputChannels[], float* const outputChannels[],
                                        UINT32 numChannels, UINT32 numFrames) {
    if (!m_isInitialized || !m_pSharedBuffer) {
        return false;
    }
    
    // The driver always expects 16-channel planar data
    // We need to handle the case where the plugin uses fewer channels
    
    float* dst = GetPluginToDriverBuffer();
    if (dst == nullptr) {
        return false;
    }
    
    // Copy plugin input to shared buffer (planar layout)
    // Driver always allocates for 16 channels, so we write to channel blocks sequentially
    for (UINT32 ch = 0; ch < LAMA_CONNECT_MAX_CHANNELS; ++ch) {
        if (ch < numChannels && inputChannels[ch] != nullptr) {
            // Copy active channel data
            memcpy(dst, inputChannels[ch], numFrames * sizeof(float));
        } else {
            // Zero unused channels
            ZeroMemory(dst, numFrames * sizeof(float));
        }
        dst += numFrames; // Move to next channel block
    }
    
    // Signal data is ready
    m_pSharedBuffer->BufferState = BUFFER_STATE_READY;
    
    // Reset completion event
    if (m_hCompletionEvent) {
        ResetEvent(m_hCompletionEvent);
    }
    
    // Trigger processing
    DWORD bytesReturned;
    BOOL ok = DeviceIoControl(m_hDevice,
                             IOCTL_LAMA_CONNECT_TRIGGER_PROCESSING,
                             &numFrames, sizeof(numFrames),
                             NULL, 0,
                             &bytesReturned,
                             NULL);
    if (!ok) {
        return false;
    }
    
    // Wait for completion
    if (m_hCompletionEvent) {
        WaitForSingleObject(m_hCompletionEvent, INFINITE);
    } else {
        // Poll buffer state
        while (m_pSharedBuffer->BufferState != BUFFER_STATE_EMPTY) {
            Sleep(0);
        }
    }
    
    // Copy output from driver
    float* src = GetDriverToPluginBuffer();
    if (src == nullptr) {
        return false;
    }
    
    // Copy from 16-channel planar data to plugin output channels
    for (UINT32 ch = 0; ch < numChannels; ++ch) {
        if (outputChannels[ch] != nullptr) {
            // Copy this channel's data (each channel block is numFrames floats)
            memcpy(outputChannels[ch], src + (ch * numFrames), numFrames * sizeof(float));
        }
    }
    
    // Signal we've consumed the output
    m_pSharedBuffer->BufferState = BUFFER_STATE_EMPTY;
    
    return true;
}

float* CLAMAConnectInterface::GetPluginToDriverBuffer() {
    if (!m_pSharedMemoryBase || !m_pSharedBuffer) return nullptr;
    BYTE* base = static_cast<BYTE*>(m_pSharedMemoryBase);
    return reinterpret_cast<float*>(base + m_pSharedBuffer->PluginToDriverBufferOffset);
}

float* CLAMAConnectInterface::GetDriverToPluginBuffer() {
    if (!m_pSharedMemoryBase || !m_pSharedBuffer) return nullptr;
    BYTE* base = static_cast<BYTE*>(m_pSharedMemoryBase);
    return reinterpret_cast<float*>(base + m_pSharedBuffer->DriverToPluginBufferOffset);
}

bool CLAMAConnectInterface::IsActive() const {
    return m_isInitialized && m_pSharedBuffer && m_pSharedBuffer->IsActive;
}