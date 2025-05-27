#include "LAMAConnectInterface.h" // Corrected to NEW.h for now
#include <windows.h>
#include <winioctl.h>
#include <ks.h>       
#include <ksmedia.h>  
#include <devguid.h> 
#include <algorithm> 
#include <iostream>  
#include <sstream>   
#include <string> // Required for std::to_wstring

// Define KSPROPERTY_CONNECTION_DATAFORMAT if not available by default
#ifndef KSPROPERTY_CONNECTION_DATAFORMAT
#define KSPROPERTY_CONNECTION_DATAFORMAT 4
#endif

#ifndef KSPROPERTY_CONNECTION_STATE
#define KSPROPERTY_CONNECTION_STATE 2
#endif

// Placeholder device path construction.
const std::wstring RENDER_DEVICE_BASENAME_CPP_NEW = L"\\\\.\\LamaLoopbackRender"; 
const std::wstring CAPTURE_DEVICE_BASENAME_CPP_NEW = L"\\\\.\\LamaLoopbackCapture";

CLAMAConnectInterface::CLAMAConnectInterface()
    : m_hRenderDevice(INVALID_HANDLE_VALUE),
      m_hCaptureDevice(INVALID_HANDLE_VALUE),
      m_SampleRate(0),
      m_FramesPerBuffer(0),
      m_PluginChannelCount(0),
      m_isInitialized(false),
      m_bIsActive(false),
      m_DeviceIndex(0) {
}

CLAMAConnectInterface::~CLAMAConnectInterface() {
    Shutdown();
}

std::wstring CLAMAConnectInterface::GetDevicePath(int deviceIndex, bool isRender) {
    std::wstringstream ss;
    if (isRender) {
        ss << RENDER_DEVICE_BASENAME_CPP_NEW << deviceIndex;
    } else {
        ss << CAPTURE_DEVICE_BASENAME_CPP_NEW << deviceIndex;
    }
    return ss.str();
}

bool CLAMAConnectInterface::Initialize(int deviceIndex) {
    if (m_isInitialized && m_DeviceIndex == deviceIndex) { // Check if already initialized for this index
        return true;
    }
    if (m_isInitialized) { // Initialized but for a different index
        Shutdown(); 
    }
    
    m_DeviceIndex = deviceIndex;
    m_lastErrorMsg.clear();

    m_DevicePathRender = GetDevicePath(m_DeviceIndex, true);
    m_DevicePathCapture = GetDevicePath(m_DeviceIndex, false);

    m_hRenderDevice = CreateFileW(
        m_DevicePathRender.c_str(),
        GENERIC_WRITE, // GENERIC_READ | GENERIC_WRITE if properties are also handled on this handle
        FILE_SHARE_WRITE | FILE_SHARE_READ, 
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 
        NULL
    );

    if (m_hRenderDevice == INVALID_HANDLE_VALUE) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: Failed to open render device: " << m_DevicePathRender << L" (Error: " << GetLastError() << L")";
        m_lastErrorMsg = ss.str();
        return false;
    }

    m_hCaptureDevice = CreateFileW(
        m_DevicePathCapture.c_str(),
        GENERIC_READ, // GENERIC_READ | GENERIC_WRITE if properties are also handled on this handle
        FILE_SHARE_READ, 
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 
        NULL
    );

    if (m_hCaptureDevice == INVALID_HANDLE_VALUE) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: Failed to open capture device: " << m_DevicePathCapture << L" (Error: " << GetLastError() << L")";
        m_lastErrorMsg = ss.str();
        CloseHandle(m_hRenderDevice);
        m_hRenderDevice = INVALID_HANDLE_VALUE;
        return false;
    }

    m_isInitialized = true;
    return true;
}

void CLAMAConnectInterface::Shutdown() {
    if (m_bIsActive) {
        stopAudio(); 
    }

    if (m_hRenderDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hRenderDevice);
        m_hRenderDevice = INVALID_HANDLE_VALUE;
    }
    if (m_hCaptureDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hCaptureDevice);
        m_hCaptureDevice = INVALID_HANDLE_VALUE;
    }
    m_isInitialized = false;
    m_bIsActive = false; 
}

bool CLAMAConnectInterface::SetDeviceIndex(int index) {
    if (m_DeviceIndex == index && m_isInitialized) return true;
    Shutdown(); 
    return Initialize(index);
}

bool CLAMAConnectInterface::startAudio(UINT32 sampleRate, UINT32 framesPerBuffer, UINT32 pluginChannelCount) {
    if (!m_isInitialized || m_hRenderDevice == INVALID_HANDLE_VALUE || m_hCaptureDevice == INVALID_HANDLE_VALUE) {
        m_lastErrorMsg = L"LAMAConnectInterface: Not initialized or device handles invalid during startAudio.";
        return false;
    }
    if (m_bIsActive) { // If already active, stop and restart with new settings
        stopAudio();
    }

    m_SampleRate = sampleRate;
    m_FramesPerBuffer = framesPerBuffer;
    m_PluginChannelCount = pluginChannelCount; // This is the number of channels JUCE uses.
    m_lastErrorMsg.clear();

    KSDATAFORMAT_WAVEFORMATEXTENSIBLE formatExt;
    ZeroMemory(&formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));

    formatExt.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    formatExt.DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    formatExt.DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    formatExt.DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    formatExt.WaveFormatEx.wFormatTag = WAVE_FORMAT_EXTENSIBLE; 
    formatExt.WaveFormatEx.nChannels = m_DriverChannelCount; // Driver always uses 16 channels
    formatExt.WaveFormatEx.nSamplesPerSec = m_SampleRate;
    formatExt.WaveFormatEx.wBitsPerSample = m_DriverBitsPerSample; 
    formatExt.WaveFormatEx.nBlockAlign = (m_DriverChannelCount * m_DriverBitsPerSample) / 8;
    formatExt.WaveFormatEx.nAvgBytesPerSec = m_SampleRate * formatExt.WaveFormatEx.nBlockAlign;
    formatExt.WaveFormatEx.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX); 

    formatExt.Samples.wValidBitsPerSample = m_DriverBitsPerSample;
    formatExt.dwChannelMask = 0; // For >2 channels, typically 0 or a specific mask.
    formatExt.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    KSP_PIN PinPropertyInstance; // Using KSP_PIN for Pin-wise properties
    ZeroMemory(&PinPropertyInstance, sizeof(KSP_PIN));
    PinPropertyInstance.Property.Set = KSPROPSETID_Connection;
    PinPropertyInstance.Property.Id = KSPROPERTY_CONNECTION_DATAFORMAT;
    PinPropertyInstance.Property.Flags = KSPROPERTY_TYPE_SET;
    PinPropertyInstance.PinId = 0; // Assuming Pin 0 for both render and capture host pins
    PinPropertyInstance.Reserved = 0;

    DWORD bytesReturned;

    if (!DeviceIoControl(m_hRenderDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN),
                         &formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), &bytesReturned, NULL)) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: Failed to set render device format (Error: " << GetLastError() << L")";
        m_lastErrorMsg = ss.str();
        return false;
    }

    if (!DeviceIoControl(m_hCaptureDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN),
                         &formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), &bytesReturned, NULL)) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: Failed to set capture device format (Error: " << GetLastError() << L")";
        m_lastErrorMsg = ss.str();
        return false;
    }
    
    KSSTATE TargetState = KSSTATE_RUN;
    PinPropertyInstance.Property.Id = KSPROPERTY_CONNECTION_STATE;

    if (!DeviceIoControl(m_hRenderDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN),
                         &TargetState, sizeof(KSSTATE), &bytesReturned, NULL)) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: Failed to set render device state to RUN (Error: " << GetLastError() << L")";
        m_lastErrorMsg = ss.str();
        return false;
    }

    if (!DeviceIoControl(m_hCaptureDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN),
                         &TargetState, sizeof(KSSTATE), &bytesReturned, NULL)) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: Failed to set capture device state to RUN (Error: " << GetLastError() << L")";
        m_lastErrorMsg = ss.str();
        TargetState = KSSTATE_STOP; // Attempt to revert render device state
        DeviceIoControl(m_hRenderDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN), &TargetState, sizeof(KSSTATE), &bytesReturned, NULL);
        return false;
    }
    
    size_t bufferSizeElements = static_cast<size_t>(m_FramesPerBuffer) * m_DriverChannelCount;
    m_DriverIOBufferRender.assign(bufferSizeElements, 0); 
    m_DriverIOBufferCapture.assign(bufferSizeElements, 0);

    m_bIsActive = true;
    return true;
}

void CLAMAConnectInterface::stopAudio() {
    if (!m_isInitialized) { 
        m_bIsActive = false;
        return;
    }
    
    bool wasPreviouslyActive = m_bIsActive; // Check if it was active to avoid error messages on benign stops
    m_bIsActive = false; 
    m_lastErrorMsg.clear();

    KSSTATE TargetState = KSSTATE_STOP;
    KSP_PIN PinPropertyInstance;
    ZeroMemory(&PinPropertyInstance, sizeof(KSP_PIN));
    PinPropertyInstance.Property.Set = KSPROPSETID_Connection;
    PinPropertyInstance.Property.Id = KSPROPERTY_CONNECTION_STATE;
    PinPropertyInstance.Property.Flags = KSPROPERTY_TYPE_SET;
    PinPropertyInstance.PinId = 0; 
    DWORD bytesReturned;

    bool renderError = false;
    bool captureError = false;

    if (m_hRenderDevice != INVALID_HANDLE_VALUE) {
        if (!DeviceIoControl(m_hRenderDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN),
                             &TargetState, sizeof(KSSTATE), &bytesReturned, NULL)) {
            if(wasPreviouslyActive) { // Only log error if we expected it to be stoppable
                renderError = true;
                std::wstringstream ss;
                ss << L"LAMAConnectInterface: Failed to set render device state to STOP (Error: " << GetLastError() << L")";
                m_lastErrorMsg = ss.str();
            }
        }
    }
    if (m_hCaptureDevice != INVALID_HANDLE_VALUE) {
        if (!DeviceIoControl(m_hCaptureDevice, IOCTL_KS_PROPERTY, &PinPropertyInstance, sizeof(KSP_PIN),
                             &TargetState, sizeof(KSSTATE), &bytesReturned, NULL)) {
            if(wasPreviouslyActive){
                captureError = true;
                std::wstringstream ss;
                ss << L"LAMAConnectInterface: Failed to set capture device state to STOP (Error: " << GetLastError() << L")";
                if (renderError) m_lastErrorMsg += L"; ";
                m_lastErrorMsg += ss.str();
            }
        }
    }
}
   
bool CLAMAConnectInterface::processAudio(float* const inputChannels[], float* const outputChannels[],
                                       UINT32 numPluginChannels, UINT32 numFrames) {
    if (!m_bIsActive || !m_isInitialized) {
        for (UINT32 i = 0; i < numPluginChannels; ++i) {
            if (outputChannels[i]) { 
                std::fill_n(outputChannels[i], numFrames, 0.0f);
            }
        }
        return false;
    }
    
    if (numFrames != m_FramesPerBuffer || numPluginChannels != m_PluginChannelCount) {
        m_lastErrorMsg = L"LAMAConnectInterface: processAudio called with mismatched frame/channel count.";
        // It's critical to fill output with silence if format doesn't match expectations
        for (UINT32 i = 0; i < numPluginChannels; ++i) {
            if (outputChannels[i]) std::fill_n(outputChannels[i], numFrames, 0.0f);
        }
        return false; 
    }
    m_lastErrorMsg.clear();

    // Ensure internal buffers are correctly sized (should be by startAudio)
    size_t requiredBufferSize = static_cast<size_t>(m_FramesPerBuffer) * m_DriverChannelCount;
    if (m_DriverIOBufferRender.size() != requiredBufferSize) m_DriverIOBufferRender.assign(requiredBufferSize, 0);
    if (m_DriverIOBufferCapture.size() != requiredBufferSize) m_DriverIOBufferCapture.assign(requiredBufferSize, 0);

    // 1. Convert and interleave float input to S16LE for the driver
    for (UINT32 frame = 0; frame < m_FramesPerBuffer; ++frame) {
        for (UINT32 chDriver = 0; chDriver < m_DriverChannelCount; ++chDriver) {
            if (chDriver < m_PluginChannelCount && inputChannels[chDriver] != nullptr) { 
                float sampleFloat = inputChannels[chDriver][frame];
                sampleFloat = std::max(-1.0f, std::min(1.0f, sampleFloat)); // Clamp
                m_DriverIOBufferRender[frame * m_DriverChannelCount + chDriver] = static_cast<INT16>(sampleFloat * 32767.0f);
            } else { 
                m_DriverIOBufferRender[frame * m_DriverChannelCount + chDriver] = 0; // Silence unused driver channels
            }
        }
    }

    UINT32 driverByteSize = m_FramesPerBuffer * m_DriverChannelCount * (m_DriverBitsPerSample / 8);
    DWORD bytesWritten = 0;

    if (!WriteFile(m_hRenderDevice, m_DriverIOBufferRender.data(), driverByteSize, &bytesWritten, NULL) || bytesWritten != driverByteSize) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: WriteFile failed or wrote partial data (Error: " << GetLastError() << L", Bytes: " << bytesWritten << L")";
        m_lastErrorMsg = ss.str();
        for (UINT32 i = 0; i < numPluginChannels; ++i) { // Silence output on error
            if (outputChannels[i]) std::fill_n(outputChannels[i], numFrames, 0.0f);
        }
        return false;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(m_hCaptureDevice, m_DriverIOBufferCapture.data(), driverByteSize, &bytesRead, NULL) || bytesRead != driverByteSize) {
        std::wstringstream ss;
        ss << L"LAMAConnectInterface: ReadFile failed or read partial data (Error: " << GetLastError() << L", Bytes: " << bytesRead << L")";
        m_lastErrorMsg = ss.str();
        for (UINT32 i = 0; i < numPluginChannels; ++i) { // Silence output on error
            if (outputChannels[i]) std::fill_n(outputChannels[i], numFrames, 0.0f);
        }
        return false;
    }

    // 2. Convert and de-interleave S16LE data from driver to float output
    for (UINT32 frame = 0; frame < m_FramesPerBuffer; ++frame) {
        for (UINT32 chPlugin = 0; chPlugin < m_PluginChannelCount; ++chPlugin) { 
            if (outputChannels[chPlugin] != nullptr) {
                 // Data from driver is interleaved: frame0ch0, frame0ch1, ... frame0ch(N-1), frame1ch0 ...
                 // We take the first m_PluginChannelCount channels from the driver's 16 channels.
                 outputChannels[chPlugin][frame] = 
                     static_cast<float>(m_DriverIOBufferCapture[frame * m_DriverChannelCount + chPlugin]) / 32768.0f;
            }
        }
    }
    return true;
}
