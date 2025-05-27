/*
 * LAMAConnectInterface.cpp
 * Production-ready interface implementation for LAMAConnect driver
 * 
 * Version: 1.1.0 - Production Release
 * Compatible with: Windows 10 19041+ / Windows 11
 * Driver Version: 1.1.0+
 */

#include "LAMAConnectInterface.h"
#include <stdio.h>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cassert>

#define LAMA_CONNECT_DEVICE_BASE_NAME L"\\\\.\\LAMAConnect"

/*
 * =================================================================
 * CONSTRUCTION AND DESTRUCTION
 * =================================================================
 */

CLAMAConnectInterface::CLAMAConnectInterface() {
    // All atomic members are default-initialized
    // Mutexes are default-constructed
    
    // Initialize performance tracking
    auto now = std::chrono::high_resolution_clock::now();
    m_lastSuccessfulOperation.store(now.time_since_epoch().count(), std::memory_order_release);
}

CLAMAConnectInterface::~CLAMAConnectInterface() {
    Shutdown();
}

CLAMAConnectInterface::CLAMAConnectInterface(CLAMAConnectInterface&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_resourceMutex);
    
    // Move atomic values
    m_isInitialized.store(other.m_isInitialized.exchange(false));
    m_deviceIndex.store(other.m_deviceIndex.load());
    m_lastError.store(other.m_lastError.load());
    m_activeChannelCount.store(other.m_activeChannelCount.load());
    m_processingTimeoutMs.store(other.m_processingTimeoutMs.load());
    m_autoRecoveryEnabled.store(other.m_autoRecoveryEnabled.load());
    
    // Move handles
    m_devicePath = std::move(other.m_devicePath);
    m_hDevice = other.m_hDevice;
    m_hSharedMemory = other.m_hSharedMemory;
    m_hCompletionEvent = other.m_hCompletionEvent;
    m_pSharedMemoryBase = other.m_pSharedMemoryBase;
    m_pSharedBuffer = other.m_pSharedBuffer;
    
    // Reset other's handles
    other.m_hDevice = INVALID_HANDLE_VALUE;
    other.m_hSharedMemory = NULL;
    other.m_hCompletionEvent = NULL;
    other.m_pSharedMemoryBase = nullptr;
    other.m_pSharedBuffer = nullptr;
    
    // Move performance counters
    m_totalFramesProcessed.store(other.m_totalFramesProcessed.exchange(0));
    m_totalBytesProcessed.store(other.m_totalBytesProcessed.exchange(0));
    m_errorCount.store(other.m_errorCount.exchange(0));
    m_overrunCount.store(other.m_overrunCount.exchange(0));
    m_underrunCount.store(other.m_underrunCount.exchange(0));
    m_lastSuccessfulOperation.store(other.m_lastSuccessfulOperation.load());
}

CLAMAConnectInterface& CLAMAConnectInterface::operator=(CLAMAConnectInterface&& other) noexcept {
    if (this != &other) {
        Shutdown();
        
        std::lock_guard<std::mutex> lock1(m_resourceMutex);
        std::lock_guard<std::mutex> lock2(other.m_resourceMutex);
        
        // Move all state (same as move constructor)
        m_isInitialized.store(other.m_isInitialized.exchange(false));
        m_deviceIndex.store(other.m_deviceIndex.load());
        m_lastError.store(other.m_lastError.load());
        m_activeChannelCount.store(other.m_activeChannelCount.load());
        m_processingTimeoutMs.store(other.m_processingTimeoutMs.load());
        m_autoRecoveryEnabled.store(other.m_autoRecoveryEnabled.load());
        
        m_devicePath = std::move(other.m_devicePath);
        m_hDevice = other.m_hDevice;
        m_hSharedMemory = other.m_hSharedMemory;
        m_hCompletionEvent = other.m_hCompletionEvent;
        m_pSharedMemoryBase = other.m_pSharedMemoryBase;
        m_pSharedBuffer = other.m_pSharedBuffer;
        
        other.m_hDevice = INVALID_HANDLE_VALUE;
        other.m_hSharedMemory = NULL;
        other.m_hCompletionEvent = NULL;
        other.m_pSharedMemoryBase = nullptr;
        other.m_pSharedBuffer = nullptr;
        
        m_totalFramesProcessed.store(other.m_totalFramesProcessed.exchange(0));
        m_totalBytesProcessed.store(other.m_totalBytesProcessed.exchange(0));
        m_errorCount.store(other.m_errorCount.exchange(0));
        m_overrunCount.store(other.m_overrunCount.exchange(0));
        m_underrunCount.store(other.m_underrunCount.exchange(0));
        m_lastSuccessfulOperation.store(other.m_lastSuccessfulOperation.load());
    }
    return *this;
}

/*
 * =================================================================
 * INITIALIZATION AND CONFIGURATION
 * =================================================================
 */

bool CLAMAConnectInterface::Initialize(int deviceIndex, DWORD timeoutMs) {
    if (m_isInitialized.load(std::memory_order_acquire)) {
        return true;
    }
    
    if (!LAMA_IS_VALID_INSTANCE_INDEX(deviceIndex)) {
        UpdateLastError(ERROR_INVALID_PARAMETER, "Invalid device index");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    
    // Double-check after acquiring lock
    if (m_isInitialized.load(std::memory_order_acquire)) {
        return true;
    }
    
    try {
        // Store device index
        m_deviceIndex.store(deviceIndex, std::memory_order_release);
        
        // Open driver device
        if (!OpenDriverDevice(deviceIndex, timeoutMs)) {
            return false;
        }
        
        // Open shared memory
        if (!OpenSharedMemory(deviceIndex)) {
            CloseAllResources();
            return false;
        }
        
        // Open completion event
        if (!OpenCompletionEvent(deviceIndex)) {
            // Not fatal - can work without event using polling
            printf("Warning: Could not open completion event, using polling fallback\n");
        }
        
        // Validate shared buffer
        if (!ValidateSharedBuffer()) {
            UpdateLastError(LAMA_ERROR_CORRUPTED_DATA, "Shared buffer validation failed");
            CloseAllResources();
            return false;
        }
        
        // Reset performance counters
        ResetPerformanceStats();
        
        // Mark as initialized
        m_isInitialized.store(true, std::memory_order_release);
        
        auto now = std::chrono::high_resolution_clock::now();
        m_lastSuccessfulOperation.store(now.time_since_epoch().count(), std::memory_order_release);
        
        printf("LAMAConnect interface initialized successfully for device %d\n", deviceIndex);
        return true;
        
    } catch (const std::exception& e) {
        UpdateLastError(ERROR_UNHANDLED_EXCEPTION, std::string("Exception during initialization: ") + e.what());
        CloseAllResources();
        return false;
    } catch (...) {
        UpdateLastError(ERROR_UNHANDLED_EXCEPTION, "Unknown exception during initialization");
        CloseAllResources();
        return false;
    }
}

void CLAMAConnectInterface::Shutdown() {
    if (!m_isInitialized.exchange(false, std::memory_order_acq_rel)) {
        return; // Already shutdown
    }
    
    printf("Shutting down LAMAConnect interface\n");
    
    // Stop any active audio first
    stopAudio();
    
    // Close all resources
    CloseAllResources();
    
    printf("LAMAConnect interface shutdown complete\n");
}

bool CLAMAConnectInterface::SetDeviceIndex(int index, DWORD timeoutMs) {
    if (!LAMA_IS_VALID_INSTANCE_INDEX(index)) {
        UpdateLastError(ERROR_INVALID_PARAMETER, "Invalid device index");
        return false;
    }
    
    if (m_deviceIndex.load(std::memory_order_acquire) == index) {
        return true; // Already using this index
    }
    
    printf("Switching to LAMAConnect device instance %d\n", index);
    
    // Shutdown current connection
    Shutdown();
    
    // Initialize with new index
    return Initialize(index, timeoutMs);
}

/*
 * =================================================================
 * AUDIO STREAMING CONTROL
 * =================================================================
 */

bool CLAMAConnectInterface::startAudio(UINT32 sampleRate, UINT32 framesPerBuffer, UINT32 channelCount) {
    if (!m_isInitialized.load(std::memory_order_acquire)) {
        UpdateLastError(ERROR_INVALID_DEVICE_STATE, "Interface not initialized");
        return false;
    }
    
    if (!ValidateParameters(sampleRate, framesPerBuffer, channelCount)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    
    try {
        printf("Starting audio: %u Hz, %u frames, %u channels\n", sampleRate, framesPerBuffer, channelCount);
        
        // Register with driver
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(m_hDevice,
                                 IOCTL_LAMA_CONNECT_REGISTER,
                                 NULL, 0,
                                 NULL, 0,
                                 &bytesReturned,
                                 NULL);
        if (!ok) {
            DWORD errorCode = ::GetLastError();
            UpdateLastError(errorCode, "IOCTL_LAMA_CONNECT_REGISTER failed");
            return false;
        }
        
        // Set format with validation
        LAMA_CONNECT_FORMAT fmt = {};
        fmt.Magic = LAMA_CONNECT_FORMAT_MAGIC;
        fmt.SampleRate = sampleRate;
        fmt.ChannelCount = channelCount;
        fmt.BufferSize = framesPerBuffer;
        
        ok = DeviceIoControl(m_hDevice,
                            IOCTL_LAMA_CONNECT_SET_FORMAT,
                            &fmt, sizeof(fmt),
                            NULL, 0,
                            &bytesReturned,
                            NULL);
        if (!ok) {
            DWORD errorCode = ::GetLastError();
            UpdateLastError(errorCode, "IOCTL_LAMA_CONNECT_SET_FORMAT failed");
            
            // Try to unregister on failure
            DeviceIoControl(m_hDevice, IOCTL_LAMA_CONNECT_UNREGISTER, NULL, 0, NULL, 0, &bytesReturned, NULL);
            return false;
        }
        
        // Store the plugin's active channel count
        m_activeChannelCount.store(channelCount, std::memory_order_release);
        
        // Validate shared buffer state
        if (!ValidateSharedBuffer()) {
            UpdateLastError(LAMA_ERROR_CORRUPTED_DATA, "Shared buffer validation failed after format set");
            stopAudio();
            return false;
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        m_lastSuccessfulOperation.store(now.time_since_epoch().count(), std::memory_order_release);
        
        printf("Audio started successfully. Driver using %d channels internally, plugin using %d channels.\n",
               LAMA_CONNECT_MAX_CHANNELS, channelCount);
        
        return true;
        
    } catch (const std::exception& e) {
        UpdateLastError(ERROR_UNHANDLED_EXCEPTION, std::string("Exception in startAudio: ") + e.what());
        return false;
    } catch (...) {
        UpdateLastError(ERROR_UNHANDLED_EXCEPTION, "Unknown exception in startAudio");
        return false;
    }
}

void CLAMAConnectInterface::stopAudio() {
    if (!m_isInitialized.load(std::memory_order_acquire)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    
    printf("Stopping audio\n");
    
    DWORD bytesReturned;
    BOOL ok = DeviceIoControl(m_hDevice,
                             IOCTL_LAMA_CONNECT_UNREGISTER,
                             NULL, 0, NULL, 0,
                             &bytesReturned, NULL);
    if (!ok) {
        DWORD errorCode = ::GetLastError();
        printf("IOCTL_LAMA_CONNECT_UNREGISTER failed (error %lu)\n", errorCode);
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
    }
}

/*
 * =================================================================
 * AUDIO PROCESSING (REAL-TIME SAFE)
 * =================================================================
 */

bool CLAMAConnectInterface::processAudio(float* const inputChannels[], float* const outputChannels[],
                                        UINT32 numChannels, UINT32 numFrames) {
    // Fast path validation - no exceptions or complex error handling in RT code
    if (!m_isInitialized.load(std::memory_order_acquire) || !m_pSharedBuffer) {
        return false;
    }
    
    // Check if driver is still active
    if (!m_pSharedBuffer->IsActive) {
        return false;
    }
    
    // Validate buffer sizes (fast checks)
    if (numFrames != m_pSharedBuffer->BufferSize) {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Validate channel count
    if (numChannels > LAMA_CONNECT_MAX_CHANNELS || numChannels == 0) {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    float* dst = GetPluginToDriverBuffer();
    if (dst == nullptr) {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Copy plugin input to shared buffer (planar layout)
    // Driver always expects 16-channel planar data
    for (UINT32 ch = 0; ch < LAMA_CONNECT_MAX_CHANNELS; ++ch) {
        if (ch < numChannels && inputChannels[ch] != nullptr) {
            // Copy active channel data
            std::memcpy(dst, inputChannels[ch], numFrames * sizeof(float));
        } else {
            // Zero unused channels
            std::memset(dst, 0, numFrames * sizeof(float));
        }
        dst += numFrames; // Move to next channel block
    }
    
    // Signal data is ready
    m_pSharedBuffer->BufferState = BUFFER_STATE_READY;
    
    // Reset completion event if available
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
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Wait for completion with timeout
    DWORD timeoutMs = m_processingTimeoutMs.load(std::memory_order_acquire);
    
    if (m_hCompletionEvent) {
        DWORD waitResult = WaitForSingleObject(m_hCompletionEvent, timeoutMs);
        if (waitResult != WAIT_OBJECT_0) {
            if (waitResult == WAIT_TIMEOUT) {
                m_errorCount.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }
    } else {
        // Poll buffer state with timeout
        auto pollStart = std::chrono::high_resolution_clock::now();
        auto timeoutDuration = std::chrono::milliseconds(timeoutMs);
        
        while (m_pSharedBuffer->BufferState != BUFFER_STATE_EMPTY) {
            auto elapsed = std::chrono::high_resolution_clock::now() - pollStart;
            if (elapsed > timeoutDuration) {
                m_errorCount.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            
            // Brief yield to prevent busy waiting
            std::this_thread::yield();
        }
    }
    
    // Copy output from driver
    float* src = GetDriverToPluginBuffer();
    if (src == nullptr) {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Copy from 16-channel planar data to plugin output channels
    for (UINT32 ch = 0; ch < numChannels; ++ch) {
        if (outputChannels[ch] != nullptr) {
            // Copy this channel's data (each channel block is numFrames floats)
            std::memcpy(outputChannels[ch], src + (ch * numFrames), numFrames * sizeof(float));
        }
    }
    
    // Signal we've consumed the output
    m_pSharedBuffer->BufferState = BUFFER_STATE_EMPTY;
    
    // Update performance counters
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    UpdatePerformanceCounters(numFrames, static_cast<UINT32>(processingTime.count()));
    
    // Update last successful operation timestamp
    m_lastSuccessfulOperation.store(endTime.time_since_epoch().count(), std::memory_order_release);
    
    return true;
}

/*
 * =================================================================
 * DIRECT BUFFER ACCESS (REAL-TIME SAFE)
 * =================================================================
 */

float* CLAMAConnectInterface::GetPluginToDriverBuffer() {
    if (!m_pSharedMemoryBase || !m_pSharedBuffer) return nullptr;
    
    // Validate offset is within bounds (fast check)
    if (m_pSharedBuffer->PluginToDriverBufferOffset >= (1024 * 1024)) { // 1MB sanity check
        return nullptr;
    }
    
    BYTE* base = static_cast<BYTE*>(m_pSharedMemoryBase);
    return reinterpret_cast<float*>(base + m_pSharedBuffer->PluginToDriverBufferOffset);
}

float* CLAMAConnectInterface::GetDriverToPluginBuffer() {
    if (!m_pSharedMemoryBase || !m_pSharedBuffer) return nullptr;
    
    // Validate offset is within bounds (fast check)
    if (m_pSharedBuffer->DriverToPluginBufferOffset >= (1024 * 1024)) { // 1MB sanity check
        return nullptr;
    }
    
    BYTE* base = static_cast<BYTE*>(m_pSharedMemoryBase);
    return reinterpret_cast<float*>(base + m_pSharedBuffer->DriverToPluginBufferOffset);
}

/*
 * =================================================================
 * STATUS AND INFORMATION
 * =================================================================
 */

bool CLAMAConnectInterface::IsActive() const {
    return m_isInitialized.load(std::memory_order_acquire) && 
           m_pSharedBuffer && 
           m_pSharedBuffer->IsActive;
}

UINT32 CLAMAConnectInterface::GetDriverSampleRate() const {
    return m_pSharedBuffer ? m_pSharedBuffer->SampleRate : 0;
}

UINT32 CLAMAConnectInterface::GetDriverBufferSize() const {
    return m_pSharedBuffer ? m_pSharedBuffer->BufferSize : 0;
}

/*
 * =================================================================
 * PERFORMANCE AND DIAGNOSTICS
 * =================================================================
 */

CLAMAConnectInterface::PerformanceStats CLAMAConnectInterface::GetPerformanceStats() const {
    PerformanceStats stats = {};
    stats.totalProcessedFrames = m_totalFramesProcessed.load(std::memory_order_acquire);
    stats.totalProcessedBytes = m_totalBytesProcessed.load(std::memory_order_acquire);
    stats.errorCount = m_errorCount.load(std::memory_order_acquire);
    stats.overrunCount = m_overrunCount.load(std::memory_order_acquire);
    stats.underrunCount = m_underrunCount.load(std::memory_order_acquire);
    stats.lastUpdateTime = std::chrono::high_resolution_clock::now();
    
    if (m_pSharedBuffer) {
        stats.averageLatencyUs = m_pSharedBuffer->AverageProcessingTimeUs;
        stats.peakLatencyUs = m_pSharedBuffer->PeakProcessingTimeUs;
    }
    
    return stats;
}

void CLAMAConnectInterface::ResetPerformanceStats() {
    m_totalFramesProcessed.store(0, std::memory_order_release);
    m_totalBytesProcessed.store(0, std::memory_order_release);
    m_errorCount.store(0, std::memory_order_release);
    m_overrunCount.store(0, std::memory_order_release);
    m_underrunCount.store(0, std::memory_order_release);
    
    if (m_pSharedBuffer) {
        m_pSharedBuffer->ProcessedFrames = 0;
        m_pSharedBuffer->ErrorCount = 0;
        m_pSharedBuffer->OverrunCount = 0;
        m_pSharedBuffer->UnderrunCount = 0;
    }
}

void CLAMAConnectInterface::PrintStatus() const {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    
    if (!m_isInitialized.load(std::memory_order_acquire)) {
        printf("LAMAConnect Interface: Not initialized\n");
        return;
    }
    
    printf("LAMAConnect Interface Status:\n");
    printf("  Device Index: %d\n", m_deviceIndex.load(std::memory_order_acquire));
    printf("  Device Path: %ls\n", m_devicePath.c_str());
    printf("  Device Handle: %p\n", m_hDevice);
    printf("  Shared Memory: %p\n", m_pSharedMemoryBase);
    printf("  Completion Event: %p\n", m_hCompletionEvent);
    
    if (m_pSharedBuffer) {
        printf("  Shared Buffer Status:\n");
        printf("    Active: %s\n", m_pSharedBuffer->IsActive ? "Yes" : "No");
        printf("    Sample Rate: %u Hz\n", m_pSharedBuffer->SampleRate);
        printf("    Channels: %u (driver) / %u (plugin active)\n", 
               m_pSharedBuffer->ChannelCount, m_activeChannelCount.load(std::memory_order_acquire));
        printf("    Buffer Size: %u frames\n", m_pSharedBuffer->BufferSize);
        printf("    Buffer State: %d\n", (int)m_pSharedBuffer->BufferState);
        printf("    Processed Frames: %llu\n", m_pSharedBuffer->ProcessedFrames);
        printf("    Error Count: %u\n", m_pSharedBuffer->ErrorCount);
        printf("    Average Latency: %u μs\n", m_pSharedBuffer->AverageProcessingTimeUs);
        printf("    Peak Latency: %u μs\n", m_pSharedBuffer->PeakProcessingTimeUs);
    }
    
    auto stats = GetPerformanceStats();
    printf("  Performance Stats:\n");
    printf("    Total Frames: %llu\n", stats.totalProcessedFrames);
    printf("    Total Bytes: %llu\n", stats.totalProcessedBytes);
    printf("    Errors: %u\n", stats.errorCount);
    printf("    Overruns: %u\n", stats.overrunCount);
    printf("    Underruns: %u\n", stats.underrunCount);
}

/*
 * =================================================================
 * ERROR HANDLING AND RECOVERY
 * =================================================================
 */

std::string CLAMAConnectInterface::GetLastErrorMessage() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastErrorMessage;
}

bool CLAMAConnectInterface::IsConnectionHealthy() const {
    if (!m_isInitialized.load(std::memory_order_acquire)) {
        return false;
    }
    
    return CheckDriverHealth();
}

bool CLAMAConnectInterface::AttemptRecovery() {
    if (!m_autoRecoveryEnabled.load(std::memory_order_acquire)) {
        return false;
    }
    
    printf("Attempting connection recovery...\n");
    
    int currentIndex = m_deviceIndex.load(std::memory_order_acquire);
    
    // Try to reinitialize with same device index
    Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(LAMAConnect::RECOVERY_DELAY_MS));
    
    bool recovered = Initialize(currentIndex);
    if (recovered) {
        printf("Connection recovery successful\n");
    } else {
        printf("Connection recovery failed\n");
    }
    
    return recovered;
}

/*
 * =================================================================
 * INTERNAL METHODS
 * =================================================================
 */

bool CLAMAConnectInterface::OpenDriverDevice(int deviceIndex, DWORD timeoutMs) {
    // Create device path
    wchar_t devPath[64];
    swprintf_s(devPath, 64, L"%s%d", LAMA_CONNECT_DEVICE_BASE_NAME, deviceIndex);
    m_devicePath = devPath;
    
    // Try to open device with retry logic
    auto startTime = std::chrono::high_resolution_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    do {
        m_hDevice = CreateFileW(m_devicePath.c_str(),
                               GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                               NULL);
        
        if (m_hDevice != INVALID_HANDLE_VALUE) {
            return true;
        }
        
        DWORD errorCode = ::GetLastError();
        
        auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
        if (elapsed >= timeout) {
            // Provide helpful error messages based on error code
            std::string errorMsg;
            switch (errorCode) {
                case ERROR_FILE_NOT_FOUND:
                    errorMsg = "Device not found. Is the LAMAConnect driver installed and loaded?";
                    break;
                case ERROR_ACCESS_DENIED:
                    errorMsg = "Access denied. Try running as administrator.";
                    break;
                case ERROR_SHARING_VIOLATION:
                    errorMsg = "Device is already in use by another application.";
                    break;
                default:
                    errorMsg = "Check if LAMAConnect driver is properly installed.";
                    break;
            }
            
            UpdateLastError(errorCode, "Failed to open device: " + errorMsg);
            return false;
        }
        
        // Brief delay before retry
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
    } while (true);
}

bool CLAMAConnectInterface::OpenSharedMemory(int deviceIndex) {
    // Try to open existing shared memory
    wchar_t memName[64];
    swprintf_s(memName, 64, L"%s%d", LAMA_CONNECT_SHARED_MEMORY_NAME, deviceIndex);
    
    m_hSharedMemory = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, memName);
    if (m_hSharedMemory == NULL) {
        DWORD errorCode = ::GetLastError();
        std::string errorMsg;
        
        if (errorCode == ERROR_FILE_NOT_FOUND) {
            errorMsg = "Shared memory not found. Driver may not be fully initialized.";
        } else {
            errorMsg = "Shared memory access failed. Check driver status.";
        }
        
        UpdateLastError(errorCode, errorMsg);
        return false;
    }
    
    // Map shared memory
    m_pSharedMemoryBase = MapViewOfFile(m_hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (m_pSharedMemoryBase == NULL) {
        DWORD errorCode = ::GetLastError();
        UpdateLastError(errorCode, "Could not map shared memory");
        CloseHandle(m_hSharedMemory);
        m_hSharedMemory = NULL;
        return false;
    }
    
    // Get shared buffer pointer
    m_pSharedBuffer = reinterpret_cast<PLAMA_CONNECT_SHARED_BUFFER>(m_pSharedMemoryBase);
    
    return true;
}

bool CLAMAConnectInterface::OpenCompletionEvent(int deviceIndex) {
    // Open completion event
    wchar_t evtName[64];
    swprintf_s(evtName, 64, L"%s%d", LAMA_CONNECT_COMPLETION_EVENT_NAME, deviceIndex);
    
    m_hCompletionEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, evtName);
    if (m_hCompletionEvent == NULL) {
        DWORD errorCode = ::GetLastError();
        // Not fatal - will use polling fallback
        printf("OpenEvent failed for '%ls' (error %lu). Using polling fallback.\n", evtName, errorCode);
        return false;
    }
    
    return true;
}

void CLAMAConnectInterface::CloseAllResources() {
    std::lock_guard<std::mutex> lock(m_resourceMutex);
    
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
    
    m_devicePath.clear();
}

bool CLAMAConnectInterface::ValidateSharedBuffer() const {
    if (!m_pSharedBuffer) {
        return false;
    }
    
    return LAMAIsValidSharedBuffer(m_pSharedBuffer);
}

bool CLAMAConnectInterface::ValidateParameters(UINT32 sampleRate, UINT32 bufferSize, UINT32 channelCount) const {
    if (!LAMA_IS_VALID_SAMPLE_RATE(sampleRate)) {
        UpdateLastError(ERROR_INVALID_PARAMETER, "Invalid sample rate");
        return false;
    }
    
    if (!LAMA_IS_VALID_BUFFER_SIZE(bufferSize)) {
        UpdateLastError(ERROR_INVALID_PARAMETER, "Invalid buffer size");
        return false;
    }
    
    if (!LAMA_IS_VALID_CHANNEL_COUNT(channelCount)) {
        UpdateLastError(ERROR_INVALID_PARAMETER, "Invalid channel count");
        return false;
    }
    
    return true;
}

void CLAMAConnectInterface::UpdateLastError(DWORD error, const std::string& message) {
    m_lastError.store(error, std::memory_order_release);
    
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastErrorMessage = message;
    
    if (!message.empty()) {
        printf("LAMAConnect Error: %s (Code: %lu)\n", message.c_str(), error);
    }
}

bool CLAMAConnectInterface::CheckDriverHealth() const {
    if (!m_pSharedBuffer) {
        return false;
    }
    
    // Check if shared buffer is still valid
    if (!LAMAIsValidSharedBuffer(m_pSharedBuffer)) {
        return false;
    }
    
    // Check if driver hasn't crashed (based on last successful operation)
    auto now = std::chrono::high_resolution_clock::now();
    auto lastSuccess = std::chrono::high_resolution_clock::time_point(
        std::chrono::high_resolution_clock::duration(
            m_lastSuccessfulOperation.load(std::memory_order_acquire)));
    
    auto timeSinceLastSuccess = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSuccess);
    
    // If it's been more than health check interval since last success, consider unhealthy
    return timeSinceLastSuccess < std::chrono::milliseconds(LAMAConnect::HEALTH_CHECK_INTERVAL_MS);
}

void CLAMAConnectInterface::UpdatePerformanceCounters(UINT32 framesProcessed, UINT32 processingTimeUs) const {
    m_totalFramesProcessed.fetch_add(framesProcessed, std::memory_order_relaxed);
    m_totalBytesProcessed.fetch_add(framesProcessed * LAMA_CONNECT_MAX_CHANNELS * sizeof(float), 
                                   std::memory_order_relaxed);
    
    if (m_pSharedBuffer) {
        // Update shared buffer statistics (driver also updates these)
        if (processingTimeUs > m_pSharedBuffer->PeakProcessingTimeUs) {
            m_pSharedBuffer->PeakProcessingTimeUs = processingTimeUs;
        }
        
        // Simple moving average for latency
        UINT32 currentAvg = m_pSharedBuffer->AverageProcessingTimeUs;
        m_pSharedBuffer->AverageProcessingTimeUs = (currentAvg + processingTimeUs) / 2;
        
        if (processingTimeUs > LAMAConnect::MAX_PROCESSING_TIME_US) {
            m_overrunCount.fetch_add(1, std::memory_order_relaxed);
            m_pSharedBuffer->OverrunCount++;
        }
    }
}

/*
 * =================================================================
 * UTILITY FUNCTIONS
 * =================================================================
 */

std::string LAMAErrorCodeToString(DWORD errorCode) {
    LPSTR messageBuffer = nullptr;
    
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);
    
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    
    // Remove trailing newlines
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    
    return message;
}

bool IsLAMAConnectDriverAvailable(int instanceIndex) {
    if (instanceIndex >= 0) {
        // Check specific instance
        if (!LAMA_IS_VALID_INSTANCE_INDEX(instanceIndex)) {
            return false;
        }
        
        wchar_t devPath[64];
        swprintf_s(devPath, 64, L"%s%d", LAMA_CONNECT_DEVICE_BASE_NAME, instanceIndex);
        
        HANDLE hDevice = CreateFileW(devPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(hDevice);
            return true;
        }
        return false;
    } else {
        // Check any instance
        for (int i = 0; i < LAMA_CONNECT_MAX_INSTANCES; ++i) {
            if (IsLAMAConnectDriverAvailable(i)) {
                return true;
            }
        }
        return false;
    }
}

std::vector<int> GetAvailableLAMAConnectInstances() {
    std::vector<int> instances;
    
    for (int i = 0; i < LAMA_CONNECT_MAX_INSTANCES; ++i) {
        if (IsLAMAConnectDriverAvailable(i)) {
            instances.push_back(i);
        }
    }
    
    return instances;
}

std::string GetLAMAConnectDriverVersion(int instanceIndex) {
    if (!IsLAMAConnectDriverAvailable(instanceIndex)) {
        return "";
    }
    
    // Could implement version query via IOCTL if supported by driver
    return "1.1.0"; // Return known version for now
}

bool CheckSystemCompatibility() {
    // Check Windows version
    OSVERSIONINFOEXW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    
    if (!GetVersionExW((OSVERSIONINFOW*)&osvi)) {
        return false;
    }
    
    // Require Windows 10 build 19041 or later
    if (osvi.dwMajorVersion < 10) {
        return false;
    }
    
    if (osvi.dwMajorVersion == 10 && osvi.dwBuildNumber < 19041) {
        return false;
    }
    
    return true;
}