#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winioctl.h> // For DeviceIoControl
#include <ks.h>
#include <ksmedia.h>
#include <cmath> // For sin
#include <sstream>   // For std::wstringstream
#include "WavWriter.h" // Assuming this is in the same directory or include path is set

// Placeholder device path construction.
const std::wstring RENDER_DEVICE_BASENAME_TEST = L"\\\\.\\LamaLoopbackRender"; 
const std::wstring CAPTURE_DEVICE_BASENAME_TEST = L"\\\\.\\LamaLoopbackCapture";

// Define KSPROPERTY_CONNECTION_DATAFORMAT if not available by default
#ifndef KSPROPERTY_CONNECTION_DATAFORMAT
#define KSPROPERTY_CONNECTION_DATAFORMAT 4
#endif

#ifndef KSPROPERTY_CONNECTION_STATE
#define KSPROPERTY_CONNECTION_STATE 2
#endif

// M_PI definition if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Define the GUID for KSPROPSETID_LamaLoopback if not already defined
// This should match the definition in your driver's common header
// {7C4E6248-4C7D-4FE4-8E4F-4E62484C7D00}
DEFINE_GUIDSTRUCT("7C4E6248-4C7D-4FE4-8E4F-4E62484C7D00", KSPROPSETID_LamaLoopback_GUID); 
#define KSPROPSETID_LamaLoopback KSPROPSETID_LamaLoopback_GUID 

typedef enum {
    KSPROPERTY_LAMA_SAMPLE_RATE = 0 // Matches the enum in lamaloopbackcommon.h
} KSPROPERTY_LAMA;

// Structure for KSPROPERTY_LAMA_SAMPLE_RATE
typedef struct {
    KSPROPERTY Property;
    ULONG      SampleRate;
} KSPROPERTY_LAMA_SAMPLE_RATE_S;


std::wstring GetDevicePath(int deviceIndex, bool isRender) {
    std::wstringstream ss;
    if (isRender) {
        ss << RENDER_DEVICE_BASENAME_TEST << deviceIndex;
    } else {
        ss << CAPTURE_DEVICE_BASENAME_TEST << deviceIndex;
    }
    return ss.str();
}

void LogError(const std::wstring& message, DWORD errorCode) {
    std::wcerr << message << L" Error Code: " << errorCode << std::endl;
}

void testSampleRateControl(int deviceIndex) {
    std::wcout << L"--- Starting Sample Rate Control Test for Instance " << deviceIndex << L" ---" << std::endl;
    std::wstring renderPath = GetDevicePath(deviceIndex, true);

    HANDLE hDevice = CreateFileW(renderPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        LogError(L"  SampleRateTest: Failed to open Render device.", GetLastError());
        return;
    }
    std::wcout << L"  SampleRateTest: Render device opened." << std::endl;

    KSPROPERTY_LAMA_SAMPLE_RATE_S srProp;
    ZeroMemory(&srProp, sizeof(srProp));
    srProp.Property.Set = KSPROPSETID_LamaLoopback; 
    srProp.Property.Id = KSPROPERTY_LAMA_SAMPLE_RATE;
    srProp.Property.Flags = KSPROPERTY_TYPE_GET;

    DWORD bytesReturned;

    // GET current sample rate
    std::wcout << L"  SampleRateTest: Attempting to GET current sample rate..." << std::endl;
    if (DeviceIoControl(hDevice, IOCTL_KS_PROPERTY, &srProp.Property, sizeof(KSPROPERTY), &srProp, sizeof(KSPROPERTY_LAMA_SAMPLE_RATE_S), &bytesReturned, NULL)) {
        if (bytesReturned >= sizeof(ULONG)) { 
             std::wcout << L"  SampleRateTest: Current sample rate GET successful. Rate: " << srProp.SampleRate << L" Hz" << std::endl;
        } else {
             std::wcout << L"  SampleRateTest: Current sample rate GET successful, but unexpected bytes returned: " << bytesReturned << std::endl;
        }
    } else {
        LogError(L"  SampleRateTest: Failed to GET current sample rate.", GetLastError());
    }

    // SET new sample rate (e.g., 44100 Hz)
    ULONG newRate = 44100;
    std::wcout << L"  SampleRateTest: Attempting to SET sample rate to " << newRate << L" Hz..." << std::endl;
    srProp.Property.Flags = KSPROPERTY_TYPE_SET;
    srProp.SampleRate = newRate;
    if (DeviceIoControl(hDevice, IOCTL_KS_PROPERTY, &srProp, sizeof(KSPROPERTY_LAMA_SAMPLE_RATE_S), NULL, 0, &bytesReturned, NULL)) {
        std::wcout << L"  SampleRateTest: SET sample rate to " << newRate << L" Hz successful." << std::endl;

        // GET again to verify
        srProp.Property.Flags = KSPROPERTY_TYPE_GET;
        std::wcout << L"  SampleRateTest: Attempting to GET sample rate again..." << std::endl;
        if (DeviceIoControl(hDevice, IOCTL_KS_PROPERTY, &srProp.Property, sizeof(KSPROPERTY), &srProp, sizeof(KSPROPERTY_LAMA_SAMPLE_RATE_S), &bytesReturned, NULL)) {
            if (bytesReturned >= sizeof(ULONG)) {
                 std::wcout << L"  SampleRateTest: Verified sample rate: " << srProp.SampleRate << L" Hz" << std::endl;
            } else {
                 std::wcout << L"  SampleRateTest: Verified sample rate GET successful, but unexpected bytes returned: " << bytesReturned << std::endl;
            }
        } else {
            LogError(L"  SampleRateTest: Failed to GET sample rate after SET.", GetLastError());
        }
    } else {
        LogError(L"  SampleRateTest: Failed to SET sample rate to " + std::to_wstring(newRate) + L" Hz.", GetLastError());
    }
    
    // SET another sample rate (e.g., 48000 Hz, assuming it's supported)
    newRate = 48000; 
    std::wcout << L"  SampleRateTest: Attempting to SET sample rate to " << newRate << L" Hz..." << std::endl;
    srProp.Property.Flags = KSPROPERTY_TYPE_SET;
    srProp.SampleRate = newRate;
    if (DeviceIoControl(hDevice, IOCTL_KS_PROPERTY, &srProp, sizeof(KSPROPERTY_LAMA_SAMPLE_RATE_S), NULL, 0, &bytesReturned, NULL)) {
        std::wcout << L"  SampleRateTest: SET sample rate to " << newRate << L" Hz successful." << std::endl;

        // GET again to verify
        srProp.Property.Flags = KSPROPERTY_TYPE_GET;
        std::wcout << L"  SampleRateTest: Attempting to GET sample rate again..." << std::endl;
        if (DeviceIoControl(hDevice, IOCTL_KS_PROPERTY, &srProp.Property, sizeof(KSPROPERTY), &srProp, sizeof(KSPROPERTY_LAMA_SAMPLE_RATE_S), &bytesReturned, NULL)) {
             if (bytesReturned >= sizeof(ULONG)) {
                 std::wcout << L"  SampleRateTest: Verified sample rate: " << srProp.SampleRate << L" Hz" << std::endl;
            } else {
                 std::wcout << L"  SampleRateTest: Verified sample rate GET successful, but unexpected bytes returned: " << bytesReturned << std::endl;
            }
        } else {
            LogError(L"  SampleRateTest: Failed to GET sample rate after SET.", GetLastError());
        }
    } else {
        LogError(L"  SampleRateTest: Failed to SET sample rate to " + std::to_wstring(newRate) + L" Hz.", GetLastError());
    }

    CloseHandle(hDevice);
    std::wcout << L"--- Sample Rate Control Test for Instance " << deviceIndex << L" Finished ---" << std::endl;
}


// Generates a sine wave and returns it as a vector of interleaved int16_t samples
std::vector<int16_t> generateSineWave(double frequency, double duration, 
                                      uint32_t sampleRate, uint16_t numChannels, double amplitude = 0.8) {
    std::vector<int16_t> audioData;
    uint32_t numSamples = static_cast<uint32_t>(sampleRate * duration);
    audioData.reserve(numSamples * numChannels);
    double phaseIncrement = 2.0 * M_PI * frequency / sampleRate;
    double currentPhase = 0.0;
    for (uint32_t i = 0; i < numSamples; ++i) {
        int16_t sampleValue = static_cast<int16_t>(amplitude * 32767.0 * sin(currentPhase));
        for (uint16_t ch = 0; ch < numChannels; ++ch) {
            audioData.push_back(sampleValue); 
        }
        currentPhase += phaseIncrement;
        if (currentPhase >= 2.0 * M_PI) {
            currentPhase -= 2.0 * M_PI;
        }
    }
    return audioData;
}

void testAudioLoopback(int deviceIndex, uint16_t numChannelsToTest, const std::string& wavFileSuffix) {
    std::wcout << L"--- Starting Audio Loopback Test (" << numChannelsToTest << L" ch) for Instance " << deviceIndex << L" ---" << std::endl;

    std::wstring renderPath = GetDevicePath(deviceIndex, true);
    std::wstring capturePath = GetDevicePath(deviceIndex, false);

    HANDLE hRenderDevice = CreateFileW(renderPath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hRenderDevice == INVALID_HANDLE_VALUE) {
        LogError(L"  LoopbackTest: Failed to open Render device.", GetLastError());
        return;
    }
    std::wcout << L"  LoopbackTest: Render device opened." << std::endl;

    HANDLE hCaptureDevice = CreateFileW(capturePath.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hCaptureDevice == INVALID_HANDLE_VALUE) {
        LogError(L"  LoopbackTest: Failed to open Capture device.", GetLastError());
        CloseHandle(hRenderDevice);
        return;
    }
    std::wcout << L"  LoopbackTest: Capture device opened." << std::endl;

    KSDATAFORMAT_WAVEFORMATEXTENSIBLE formatExt;
    ZeroMemory(&formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
    formatExt.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    formatExt.DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    formatExt.DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    formatExt.DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
    formatExt.WaveFormatEx.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    formatExt.WaveFormatEx.nChannels = numChannelsToTest;
    formatExt.WaveFormatEx.nSamplesPerSec = 48000;
    formatExt.WaveFormatEx.wBitsPerSample = 16;
    formatExt.WaveFormatEx.nBlockAlign = (formatExt.WaveFormatEx.nChannels * formatExt.WaveFormatEx.wBitsPerSample) / 8;
    formatExt.WaveFormatEx.nAvgBytesPerSec = formatExt.WaveFormatEx.nSamplesPerSec * formatExt.WaveFormatEx.nBlockAlign;
    formatExt.WaveFormatEx.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    formatExt.Samples.wValidBitsPerSample = 16;
    formatExt.dwChannelMask = (numChannelsToTest == 2) ? KSAUDIO_SPEAKER_STEREO : 0; 
    formatExt.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    KSP_PIN PinProperty;
    ZeroMemory(&PinProperty, sizeof(KSP_PIN));
    PinProperty.Property.Set = KSPROPSETID_Connection;
    PinProperty.Property.Id = KSPROPERTY_CONNECTION_DATAFORMAT;
    PinProperty.Property.Flags = KSPROPERTY_TYPE_SET;
    PinProperty.PinId = 0; 
    DWORD bytesReturnedPin;

    std::wcout << L"  LoopbackTest: Setting DATAFORMAT (" << numChannelsToTest << " ch)..." << std::endl;
    if (!DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), &bytesReturnedPin, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Render DATAFORMAT.", GetLastError());
        goto cleanup;
    }
    if (!DeviceIoControl(hCaptureDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), &bytesReturnedPin, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Capture DATAFORMAT.", GetLastError());
        goto cleanup;
    }
    std::wcout << L"  LoopbackTest: DATAFORMAT set." << std::endl;

    KSSTATE targetState = KSSTATE_RUN;
    PinProperty.Property.Id = KSPROPERTY_CONNECTION_STATE;
    std::wcout << L"  LoopbackTest: Setting state to RUN..." << std::endl;
    if (!DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturnedPin, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Render state to RUN.", GetLastError());
        goto cleanup;
    }
    if (!DeviceIoControl(hCaptureDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturnedPin, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Capture state to RUN.", GetLastError());
        targetState = KSSTATE_STOP; 
        DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturnedPin, NULL);
        goto cleanup;
    }
    std::wcout << L"  LoopbackTest: State set to RUN." << std::endl;

    uint32_t sampleRate = formatExt.WaveFormatEx.nSamplesPerSec;
    uint16_t bitsPerSample = formatExt.WaveFormatEx.wBitsPerSample;
    double durationSeconds = 2.0;
    std::vector<int16_t> sineWave = generateSineWave(440.0, durationSeconds, sampleRate, numChannelsToTest);
    DWORD dataByteSize = static_cast<DWORD>(sineWave.size() * sizeof(int16_t));

    std::vector<int16_t> capturedData;
    capturedData.resize(sineWave.size()); 

    DWORD totalBytesWritten = 0;
    DWORD totalBytesRead = 0;
    UINT32 bufferDurationMs = 100; 
    DWORD chunkSizeSamples = (sampleRate * bufferDurationMs / 1000) * numChannelsToTest;
    DWORD chunkSizeBytes = chunkSizeSamples * sizeof(int16_t);

    char* pCurrentSinePos = reinterpret_cast<char*>(sineWave.data());
    char* pCurrentCapturePos = reinterpret_cast<char*>(capturedData.data());
    DWORD remainingBytesToWrite = dataByteSize;
    DWORD remainingBytesToRead = dataByteSize;

    std::wcout << L"  LoopbackTest: Starting audio loopback (" << numChannelsToTest << " ch)..." << std::endl;
    while (totalBytesRead < dataByteSize && totalBytesWritten < dataByteSize) {
        DWORD bytesToWriteThisChunk = min(chunkSizeBytes, remainingBytesToWrite);
        DWORD bytesWrittenCurrent = 0; 
        if (bytesToWriteThisChunk > 0) {
            if (!WriteFile(hRenderDevice, pCurrentSinePos, bytesToWriteThisChunk, &bytesWrittenCurrent, NULL)) {
                LogError(L"  LoopbackTest: WriteFile failed.", GetLastError());
                break;
            }
            pCurrentSinePos += bytesWrittenCurrent;
            totalBytesWritten += bytesWrittenCurrent;
            remainingBytesToWrite -= bytesWrittenCurrent;
        }

        DWORD bytesToReadThisChunk = min(chunkSizeBytes, remainingBytesToRead);
        if (bytesToReadThisChunk > 0 && totalBytesWritten > totalBytesRead) { 
            DWORD bytesReadCurrent = 0; 
            if (!ReadFile(hCaptureDevice, pCurrentCapturePos, bytesToReadThisChunk, &bytesReadCurrent, NULL)) {
                LogError(L"  LoopbackTest: ReadFile failed.", GetLastError());
                break;
            }
            pCurrentCapturePos += bytesReadCurrent;
            totalBytesRead += bytesReadCurrent;
            remainingBytesToRead -= bytesReadCurrent;
        }
    }
    std::wcout << L"  LoopbackTest: Loopback finished. Total Written: " << totalBytesWritten << ", Total Read: " << totalBytesRead << std::endl;

    if (totalBytesRead > 0) { 
        capturedData.resize(totalBytesRead / sizeof(int16_t)); 
        std::string wavFilename = "captured_audio_instance_" + std::to_string(deviceIndex) + wavFileSuffix + ".wav";
        if (WavWriter::writeWavFile(wavFilename, capturedData, numChannelsToTest, sampleRate, bitsPerSample)) {
            std::wcout << L"  LoopbackTest: Captured audio saved to " << wavFilename.c_str() << std::endl;
        } else {
            std::wcerr << L"  LoopbackTest: Error saving WAV file." << std::endl;
        }
    } else {
        std::wcout << L"  LoopbackTest: No data read, WAV file not saved." << std::endl;
    }

cleanup:
    std::wcout << L"  LoopbackTest: Cleaning up..." << std::endl;
    targetState = KSSTATE_STOP;
    PinProperty.Property.Id = KSPROPERTY_CONNECTION_STATE;
    if (hRenderDevice != INVALID_HANDLE_VALUE) {
        DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturnedPin, NULL);
        CloseHandle(hRenderDevice);
    }
    if (hCaptureDevice != INVALID_HANDLE_VALUE) {
        DeviceIoControl(hCaptureDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturnedPin, NULL);
        CloseHandle(hCaptureDevice);
    }
    std::wcout << L"--- Audio Loopback Test (" << numChannelsToTest << L" ch) for Instance " << deviceIndex << L" Finished ---" << std::endl;
}

void testAudioLoopback16ch(int deviceIndex) {
    testAudioLoopback(deviceIndex, 16, "_16ch");
}

int main() {
    std::wcout << L"Lama Loopback Driver Test Application" << std::endl;
    std::wcout << L"=====================================" << std::endl << std::endl;

    bool anyDeviceOpened = false;

    for (int i = 0; i < 1; ++i) { 
        std::wcout << L"Testing Driver Instance: " << i << std::endl;
        std::wstring renderPath = GetDevicePath(i, true);
        std::wstring capturePath = GetDevicePath(i, false);

        std::wcout << L"  Render Path: " << renderPath << std::endl;
        std::wcout << L"  Capture Path: " << capturePath << std::endl;

        HANDLE hRender = CreateFileW(renderPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hRender == INVALID_HANDLE_VALUE) {
            LogError(L"    Failed to open Render device.", GetLastError());
        } else {
            std::wcout << L"    Render device opened successfully." << std::endl;
            CloseHandle(hRender);
            anyDeviceOpened = true;
        }

        HANDLE hCapture = CreateFileW(capturePath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hCapture == INVALID_HANDLE_VALUE) {
            LogError(L"    Failed to open Capture device.", GetLastError());
        } else {
            std::wcout << L"    Capture device opened successfully." << std::endl;
            CloseHandle(hCapture);
            anyDeviceOpened = true;
        }
        std::wcout << L"------------------------------------" << std::endl;
    }

    if (!anyDeviceOpened && 0 ) { 
        std::wcerr << L"Initial check: Instance 0 could not be opened. Please ensure the driver is installed and running." << std::endl;
    }

    testSampleRateControl(0);
    std::wcout << L"------------------------------------" << std::endl;

    testAudioLoopback(0, 2, "_2ch"); 
    std::wcout << L"------------------------------------" << std::endl;

    testAudioLoopback16ch(0); 

    std::wcout << L"Driver Test Application Finished." << std::endl;
    return 0;
}
