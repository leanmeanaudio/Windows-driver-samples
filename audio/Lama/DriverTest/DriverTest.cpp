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
            audioData.push_back(sampleValue); // Same sample for all channels (mono to stereo/N-ch)
        }
        currentPhase += phaseIncrement;
        if (currentPhase >= 2.0 * M_PI) {
            currentPhase -= 2.0 * M_PI;
        }
    }
    return audioData;
}

void testAudioLoopback(int deviceIndex) {
    std::wcout << L"--- Starting Audio Loopback Test for Instance " << deviceIndex << L" ---" << std::endl;

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

    // Define audio format
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE formatExt;
    ZeroMemory(&formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
    formatExt.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    formatExt.DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    formatExt.DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    formatExt.DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
    formatExt.WaveFormatEx.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    formatExt.WaveFormatEx.nChannels = 2; // Stereo for this test
    formatExt.WaveFormatEx.nSamplesPerSec = 48000;
    formatExt.WaveFormatEx.wBitsPerSample = 16;
    formatExt.WaveFormatEx.nBlockAlign = (formatExt.WaveFormatEx.nChannels * formatExt.WaveFormatEx.wBitsPerSample) / 8;
    formatExt.WaveFormatEx.nAvgBytesPerSec = formatExt.WaveFormatEx.nSamplesPerSec * formatExt.WaveFormatEx.nBlockAlign;
    formatExt.WaveFormatEx.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    formatExt.Samples.wValidBitsPerSample = 16;
    formatExt.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    formatExt.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    KSP_PIN PinProperty;
    ZeroMemory(&PinProperty, sizeof(KSP_PIN));
    PinProperty.Property.Set = KSPROPSETID_Connection;
    PinProperty.Property.Id = KSPROPERTY_CONNECTION_DATAFORMAT;
    PinProperty.Property.Flags = KSPROPERTY_TYPE_SET;
    PinProperty.PinId = 0; // Assuming Pin 0
    DWORD bytesReturned;

    std::wcout << L"  LoopbackTest: Setting DATAFORMAT..." << std::endl;
    if (!DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), &bytesReturned, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Render DATAFORMAT.", GetLastError());
        goto cleanup;
    }
    if (!DeviceIoControl(hCaptureDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &formatExt, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), &bytesReturned, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Capture DATAFORMAT.", GetLastError());
        goto cleanup;
    }
    std::wcout << L"  LoopbackTest: DATAFORMAT set." << std::endl;

    KSSTATE targetState = KSSTATE_RUN;
    PinProperty.Property.Id = KSPROPERTY_CONNECTION_STATE;
    std::wcout << L"  LoopbackTest: Setting state to RUN..." << std::endl;
    if (!DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturned, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Render state to RUN.", GetLastError());
        goto cleanup;
    }
    if (!DeviceIoControl(hCaptureDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturned, NULL)) {
        LogError(L"  LoopbackTest: Failed to set Capture state to RUN.", GetLastError());
        targetState = KSSTATE_STOP; // Try to stop render pin
        DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturned, NULL);
        goto cleanup;
    }
    std::wcout << L"  LoopbackTest: State set to RUN." << std::endl;

    // Audio Data Generation
    uint32_t sampleRate = 48000;
    uint16_t numChannels = 2;
    uint16_t bitsPerSample = 16;
    double durationSeconds = 2.0;
    std::vector<int16_t> sineWave = generateSineWave(440.0, durationSeconds, sampleRate, numChannels);
    DWORD dataByteSize = static_cast<DWORD>(sineWave.size() * sizeof(int16_t));

    // Audio Loopback
    std::vector<int16_t> capturedData;
    capturedData.resize(sineWave.size()); // Allocate space for captured audio

    DWORD totalBytesWritten = 0;
    DWORD totalBytesRead = 0;
    UINT32 bufferDurationMs = 100; // Process in 100ms chunks
    DWORD chunkSizeSamples = (sampleRate * bufferDurationMs / 1000) * numChannels;
    DWORD chunkSizeBytes = chunkSizeSamples * sizeof(int16_t);

    char* pCurrentSinePos = reinterpret_cast<char*>(sineWave.data());
    char* pCurrentCapturePos = reinterpret_cast<char*>(capturedData.data());
    DWORD remainingBytesToWrite = dataByteSize;
    DWORD remainingBytesToRead = dataByteSize;

    std::wcout << L"  LoopbackTest: Starting audio loopback..." << std::endl;
    while (totalBytesRead < dataByteSize && totalBytesWritten < dataByteSize) {
        DWORD bytesToWriteThisChunk = min(chunkSizeBytes, remainingBytesToWrite);
        DWORD bytesWrittenCurrent = 0; // Renamed to avoid conflict
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
         // Only read if we've written something or expect data anyway
        if (bytesToReadThisChunk > 0 && totalBytesWritten > totalBytesRead) { // Ensure there's likely data to read
            DWORD bytesReadCurrent = 0; // Renamed
            if (!ReadFile(hCaptureDevice, pCurrentCapturePos, bytesToReadThisChunk, &bytesReadCurrent, NULL)) {
                LogError(L"  LoopbackTest: ReadFile failed.", GetLastError());
                break;
            }
            pCurrentCapturePos += bytesReadCurrent;
            totalBytesRead += bytesReadCurrent;
            remainingBytesToRead -= bytesReadCurrent;
        }
        // If WriteFile is blocking and ReadFile is blocking, this loop should progress.
        // If non-blocking I/O was used, a Sleep or event synchronization would be critical here.
        // For this test, assuming blocking I/O or driver handles timing.
        // Sleep(1); // Can add a small sleep if issues with 100% CPU usage occur
    }
    std::wcout << L"  LoopbackTest: Loopback finished. Total Written: " << totalBytesWritten << ", Total Read: " << totalBytesRead << std::endl;

    // Save Captured Audio
    if (totalBytesRead > 0) { // Only save if something was read
        // Resize capturedData to actual bytes read if it's different (though loop aims for same size)
        capturedData.resize(totalBytesRead / sizeof(int16_t)); 
        std::string wavFilename = "captured_audio_instance_" + std::to_string(deviceIndex) + ".wav";
        if (WavWriter::writeWavFile(wavFilename, capturedData, numChannels, sampleRate, bitsPerSample)) {
            std::wcout << L"  LoopbackTest: Captured audio saved to " << wavFilename.c_str() << std::endl;
        } else {
            std::wcerr << L"  LoopbackTest: Error saving WAV file." << std::endl;
        }
    } else {
        std::wcout << L"  LoopbackTest: No data read, WAV file not saved." << std::endl;
    }

cleanup:
    // Cleanup
    std::wcout << L"  LoopbackTest: Cleaning up..." << std::endl;
    targetState = KSSTATE_STOP;
    PinProperty.Property.Id = KSPROPERTY_CONNECTION_STATE;
    if (hRenderDevice != INVALID_HANDLE_VALUE) {
        DeviceIoControl(hRenderDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturned, NULL);
        CloseHandle(hRenderDevice);
    }
    if (hCaptureDevice != INVALID_HANDLE_VALUE) {
        DeviceIoControl(hCaptureDevice, IOCTL_KS_PROPERTY, &PinProperty, sizeof(KSP_PIN), &targetState, sizeof(KSSTATE), &bytesReturned, NULL);
        CloseHandle(hCaptureDevice);
    }
    std::wcout << L"--- Audio Loopback Test for Instance " << deviceIndex << L" Finished ---" << std::endl;
}


int main() {
    std::wcout << L"Lama Loopback Driver Test Application" << std::endl;
    std::wcout << L"=====================================" << std::endl << std::endl;

    bool anyDeviceOpened = false;

    // Test Driver Detection/Opening for 4 instances
    for (int i = 0; i < 1; ++i) { // Test only instance 0 for brevity during test
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

    if (!anyDeviceOpened && 0 /*Change to test only one instance for now*/) { // Adjusted for single instance test
        std::wcerr << L"Initial check: Instance 0 could not be opened. Please ensure the driver is installed and running." << std::endl;
        // return -1; // Allow loopback test to proceed and show its own errors
    }

    // Test Format Setting and State Change on Instance 0 (already part of testAudioLoopback)
    // This section can be removed or commented if testAudioLoopback covers it sufficiently.
    /*
    std::wcout << L"Testing Format/State on Instance 0 (pre-loopback test)..." << std::endl;
    // ... (previous format/state test code from initial version) ...
    std::wcout << L"------------------------------------" << std::endl;
    */

    // Perform Audio Loopback Test on Instance 0
    testAudioLoopback(0);


    std::wcout << L"Driver Test Application Finished." << std::endl;
    return 0;
}
