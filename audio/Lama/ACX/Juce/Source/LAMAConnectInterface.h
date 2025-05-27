#pragma once

/*
 * LAMAConnectInterface.h
 * Production-ready interface for communicating with the LAMAConnect driver
 * 
 * Version: 1.1.0 - Production Release
 * Compatible with: Windows 10 19041+ / Windows 11
 * Driver Version: 1.1.0+
 */

#include <Windows.h>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include "LAMAConnectShared.h"

/**
 * Interface class for communicating with the LAMAConnect driver
 * 
 * The driver always uses 16 channels internally for maximum compatibility,
 * but the plugin can use fewer channels (2-16). The interface handles
 * the conversion between plugin channel count and driver's fixed 16-channel format.
 * 
 * Thread Safety: This class is thread-safe for concurrent access.
 * Performance: Optimized for real-time audio processing with minimal latency.
 * Error Handling: Comprehensive error checking and recovery mechanisms.
 * 
 * Usage:
 *   1. Create instance: auto interface = std::make_unique<CLAMAConnectInterface>();
 *   2. Initialize: interface->Initialize(deviceIndex);
 *   3. Start audio: interface->startAudio(sampleRate, bufferSize, channelCount);
 *   4. Process blocks: interface->processAudio(inputs, outputs, channels, frames);
 *   5. Stop audio: interface->stopAudio();
 *   6. Cleanup: interface->Shutdown() or destructor
 */
class CLAMAConnectInterface {
public:
    /*
     * =================================================================
     * CONSTRUCTION AND DESTRUCTION
     * =================================================================
     */
    
    CLAMAConnectInterface();
    ~CLAMAConnectInterface();

    /*
     * =================================================================
     * INITIALIZATION AND CONFIGURATION
     * =================================================================
     */
    
    /**
     * Initialize connection to specific driver instance
     * @param deviceIndex Driver instance index (0-3, default 0)
     * @param timeoutMs Connection timeout in milliseconds (default 5000)
     * @return true if successful, false if driver not found or inaccessible
     * 
     * Thread Safety: Not thread-safe during initialization
     * Performance: May block for up to timeoutMs milliseconds
     */
    bool Initialize(int deviceIndex = 0, DWORD timeoutMs = 5000);
    
    /**
     * Shutdown connection and cleanup all resources
     * Thread Safety: Thread-safe, can be called from any thread
     * Performance: May block briefly to ensure clean shutdown
     */
    void Shutdown();

    /**
     * Switch to different driver instance
     * @param index New driver instance index (0-3)
     * @param timeoutMs Connection timeout in milliseconds (default 5000)
     * @return true if successful
     * 
     * Thread Safety: Not thread-safe during instance switching
     * Performance: May block for up to timeoutMs milliseconds
     */
    bool SetDeviceIndex(int index, DWORD timeoutMs = 5000);

    /*
     * =================================================================
     * AUDIO STREAMING CONTROL
     * =================================================================
     */
    
    /**
     * Start audio streaming with specified format
     * @param sampleRate Sample rate in Hz (8000-192000)
     * @param framesPerBuffer Number of frames per audio buffer (32-8192)
     * @param channelCount Plugin's active channel count (1-16)
     * @return true if successful
     * 
     * Note: Driver always uses 16 channels internally, channelCount is the plugin's active count
     * Thread Safety: Not thread-safe during start/stop operations
     * Performance: May block briefly during initialization
     */
    bool startAudio(UINT32 sampleRate, UINT32 framesPerBuffer, UINT32 channelCount);
    
    /**
     * Stop audio streaming
     * Thread Safety: Thread-safe, can be called from any thread
     * Performance: Non-blocking, immediate shutdown
     */
    void stopAudio();

    /*
     * =================================================================
     * AUDIO PROCESSING (REAL-TIME SAFE)
     * =================================================================
     */
    
    /**
     * Process one audio block (REAL-TIME SAFE)
     * @param inputChannels Array of input channel buffers (plugin's active channels)
     * @param outputChannels Array of output channel buffers (plugin's active channels)
     * @param numChannels Number of channels (must match value from startAudio)
     * @param numFrames Number of frames (must match value from startAudio)
     * @return true if successful
     * 
     * The driver handles conversion to/from its internal 16-channel format.
     * Input/output arrays should have numChannels elements.
     * Each channel buffer should contain numFrames float samples.
     * 
     * Thread Safety: Thread-safe for audio processing thread
     * Performance: Optimized for real-time use, no dynamic allocations
     * Latency: Typically < 1ms processing time
     */
    bool processAudio(float* const inputChannels[], float* const outputChannels[],
                      UINT32 numChannels, UINT32 numFrames);

    /*
     * =================================================================
     * DIRECT BUFFER ACCESS (ADVANCED USE)
     * =================================================================
     */
    
    /**
     * Get direct pointer to plugin-to-driver buffer (REAL-TIME SAFE)
     * @return Pointer to 16-channel planar float buffer, or nullptr if not available
     * Buffer layout: [ch0_frame0, ch0_frame1, ..., ch0_frameN, ch1_frame0, ch1_frame1, ...]
     * 
     * Thread Safety: Thread-safe for audio processing thread
     * Performance: Zero-copy access to driver buffer
     */
    float* GetPluginToDriverBuffer();
    
    /**
     * Get direct pointer to driver-to-plugin buffer (REAL-TIME SAFE)
     * @return Pointer to 16-channel planar float buffer, or nullptr if not available
     * Buffer layout: [ch0_frame0, ch0_frame1, ..., ch0_frameN, ch1_frame0, ch1_frame1, ...]
     * 
     * Thread Safety: Thread-safe for audio processing thread
     * Performance: Zero-copy access to driver buffer
     */
    float* GetDriverToPluginBuffer();

    /*
     * =================================================================
     * STATUS AND INFORMATION
     * =================================================================
     */
    
    /**
     * Check if interface is initialized
     * @return true if Initialize() was successful and Shutdown() hasn't been called
     * Thread Safety: Thread-safe
     * Performance: Atomic operation, very fast
     */
    bool IsInitialized() const { return m_isInitialized.load(std::memory_order_acquire); }
    
    /**
     * Check if audio is currently active
     * @return true if startAudio() was successful and audio is streaming
     * Thread Safety: Thread-safe
     * Performance: Fast, checks shared buffer state
     */
    bool IsActive() const;
    
    /**
     * Get the plugin's active channel count (not the driver's fixed 16)
     * @return Number of channels the plugin is using (1-16)
     * Thread Safety: Thread-safe
     * Performance: Atomic operation, very fast
     */
    UINT32 GetActiveChannelCount() const { return m_activeChannelCount.load(std::memory_order_acquire); }
    
    /**
     * Get current sample rate from driver
     * @return Sample rate in Hz, or 0 if not available
     * Thread Safety: Thread-safe
     * Performance: Fast, reads from shared buffer
     */
    UINT32 GetDriverSampleRate() const;
    
    /**
     * Get current buffer size from driver
     * @return Buffer size in frames, or 0 if not available
     * Thread Safety: Thread-safe
     * Performance: Fast, reads from shared buffer
     */
    UINT32 GetDriverBufferSize() const;
    
    /**
     * Get current driver instance index
     * @return Driver instance index (0-3)
     * Thread Safety: Thread-safe
     * Performance: Atomic operation, very fast
     */
    int GetDriverInstanceIndex() const { return m_deviceIndex.load(std::memory_order_acquire); }

    /*
     * =================================================================
     * PERFORMANCE AND DIAGNOSTICS
     * =================================================================
     */
    
    /**
     * Get performance statistics
     * @return Performance stats structure
     * Thread Safety: Thread-safe
     * Performance: Fast, reads from cached values
     */
    struct PerformanceStats {
        UINT64 totalProcessedFrames;
        UINT64 totalProcessedBytes;
        UINT32 averageLatencyUs;
        UINT32 peakLatencyUs;
        UINT32 errorCount;
        UINT32 overrunCount;
        UINT32 underrunCount;
        std::chrono::high_resolution_clock::time_point lastUpdateTime;
    };
    
    PerformanceStats GetPerformanceStats() const;
    
    /**
     * Reset performance statistics
     * Thread Safety: Thread-safe
     * Performance: Fast, atomic reset
     */
    void ResetPerformanceStats();
    
    /**
     * Print detailed status information to debug output (for debugging)
     * Thread Safety: Thread-safe
     * Performance: Slow, only for debugging use
     */
    void PrintStatus() const;

    /*
     * =================================================================
     * ERROR HANDLING AND RECOVERY
     * =================================================================
     */
    
    /**
     * Get last Windows error code from failed operations
     * @return GetLastError() value from most recent failed Win32 call
     * Thread Safety: Thread-safe
     * Performance: Fast, reads cached value
     */
    DWORD GetLastError() const { return m_lastError.load(std::memory_order_acquire); }
    
    /**
     * Get detailed error message for last failed operation
     * @return Human-readable error description
     * Thread Safety: Thread-safe
     * Performance: Fast, returns cached string
     */
    std::string GetLastErrorMessage() const;
    
    /**
     * Check if driver connection is healthy
     * @return true if driver is responding normally
     * Thread Safety: Thread-safe
     * Performance: Fast validation check
     */
    bool IsConnectionHealthy() const;
    
    /**
     * Attempt to recover from connection errors
     * @return true if recovery was successful
     * Thread Safety: Not thread-safe during recovery
     * Performance: May block during recovery attempt
     */
    bool AttemptRecovery();

    /*
     * =================================================================
     * ADVANCED CONFIGURATION
     * =================================================================
     */
    
    /**
     * Set processing timeout for audio operations
     * @param timeoutMs Timeout in milliseconds (default 1000)
     * Thread Safety: Thread-safe
     * Performance: Fast, stores value atomically
     */
    void SetProcessingTimeout(DWORD timeoutMs) { 
        m_processingTimeoutMs.store(timeoutMs, std::memory_order_release); 
    }
    
    /**
     * Get current processing timeout
     * @return Timeout in milliseconds
     * Thread Safety: Thread-safe
     * Performance: Atomic read, very fast
     */
    DWORD GetProcessingTimeout() const { 
        return m_processingTimeoutMs.load(std::memory_order_acquire); 
    }
    
    /**
     * Enable/disable automatic error recovery
     * @param enable true to enable automatic recovery
     * Thread Safety: Thread-safe
     * Performance: Fast, atomic store
     */
    void SetAutoRecovery(bool enable) { 
        m_autoRecoveryEnabled.store(enable, std::memory_order_release); 
    }
    
    /**
     * Check if automatic error recovery is enabled
     * @return true if auto-recovery is enabled
     * Thread Safety: Thread-safe
     * Performance: Atomic read, very fast
     */
    bool IsAutoRecoveryEnabled() const { 
        return m_autoRecoveryEnabled.load(std::memory_order_acquire); 
    }

private:
    /*
     * =================================================================
     * INTERNAL STATE (THREAD-SAFE)
     * =================================================================
     */
    
    // Connection state
    std::atomic<bool> m_isInitialized{false};
    std::atomic<int> m_deviceIndex{0};
    std::atomic<DWORD> m_lastError{0};
    std::atomic<UINT32> m_activeChannelCount{2};
    
    // Configuration
    std::atomic<DWORD> m_processingTimeoutMs{1000};
    std::atomic<bool> m_autoRecoveryEnabled{true};
    
    // Handles and resources (protected by mutex)
    mutable std::mutex m_resourceMutex;
    std::wstring m_devicePath;
    HANDLE m_hDevice{INVALID_HANDLE_VALUE};
    HANDLE m_hSharedMemory{NULL};
    HANDLE m_hCompletionEvent{NULL};
    void* m_pSharedMemoryBase{nullptr};
    PLAMA_CONNECT_SHARED_BUFFER m_pSharedBuffer{nullptr};
    
    // Performance tracking (atomic for thread safety)
    mutable std::atomic<UINT64> m_totalFramesProcessed{0};
    mutable std::atomic<UINT64> m_totalBytesProcessed{0};
    mutable std::atomic<UINT32> m_errorCount{0};
    mutable std::atomic<UINT32> m_overrunCount{0};
    mutable std::atomic<UINT32> m_underrunCount{0};
    
    // Error message cache (protected by mutex)
    mutable std::mutex m_errorMutex;
    mutable std::string m_lastErrorMessage;
    
    // Connection health tracking
    mutable std::atomic<std::chrono::high_resolution_clock::rep> m_lastSuccessfulOperation{0};
    
    /*
     * =================================================================
     * INTERNAL METHODS
     * =================================================================
     */
    
    // Resource management
    bool OpenDriverDevice(int deviceIndex, DWORD timeoutMs);
    bool OpenSharedMemory(int deviceIndex);
    bool OpenCompletionEvent(int deviceIndex);
    void CloseAllResources();
    
    // Validation and error handling
    bool ValidateSharedBuffer() const;
    bool ValidateParameters(UINT32 sampleRate, UINT32 bufferSize, UINT32 channelCount) const;
    void UpdateLastError(DWORD error, const std::string& message = "");
    bool CheckDriverHealth() const;
    
    // Performance tracking
    void UpdatePerformanceCounters(UINT32 framesProcessed, UINT32 processingTimeUs) const;
    
    /*
     * =================================================================
     * DISABLED OPERATIONS
     * =================================================================
     */
    
    // Disable copy/assignment to prevent handle duplication issues
    CLAMAConnectInterface(const CLAMAConnectInterface&) = delete;
    CLAMAConnectInterface& operator=(const CLAMAConnectInterface&) = delete;
    
    // Enable move operations for efficiency
    CLAMAConnectInterface(CLAMAConnectInterface&& other) noexcept;
    CLAMAConnectInterface& operator=(CLAMAConnectInterface&& other) noexcept;
};

/*
 * =================================================================
 * UTILITY FUNCTIONS
 * =================================================================
 */

/**
 * Convert Windows error code to human-readable string
 * @param errorCode Windows error code
 * @return Human-readable error description
 */
std::string LAMAErrorCodeToString(DWORD errorCode);

/**
 * Check if LAMAConnect driver is installed and available
 * @param instanceIndex Driver instance to check (0-3, -1 for any)
 * @return true if driver is available
 */
bool IsLAMAConnectDriverAvailable(int instanceIndex = -1);

/**
 * Get list of available LAMAConnect driver instances
 * @return Vector of available instance indices
 */
std::vector<int> GetAvailableLAMAConnectInstances();

/**
 * Get driver version information
 * @param instanceIndex Driver instance to query (default 0)
 * @return Version string, or empty if not available
 */
std::string GetLAMAConnectDriverVersion(int instanceIndex = 0);

/**
 * Check if system meets minimum requirements for LAMAConnect
 * @return true if system is compatible
 */
bool CheckSystemCompatibility();

/*
 * =================================================================
 * CONSTANTS AND LIMITS
 * =================================================================
 */

namespace LAMAConnect {
    // API version
    constexpr UINT32 API_VERSION_MAJOR = 1;
    constexpr UINT32 API_VERSION_MINOR = 1;
    constexpr UINT32 API_VERSION_BUILD = 0;
    
    // Performance constants
    constexpr UINT32 RECOMMENDED_BUFFER_SIZE = 512;
    constexpr UINT32 RECOMMENDED_SAMPLE_RATE = 48000;
    constexpr UINT32 MAX_PROCESSING_TIME_US = 5000;  // 5ms max processing time
    
    // Timeout constants
    constexpr DWORD DEFAULT_CONNECTION_TIMEOUT_MS = 5000;
    constexpr DWORD DEFAULT_PROCESSING_TIMEOUT_MS = 1000;
    constexpr DWORD HEALTH_CHECK_INTERVAL_MS = 1000;
    
    // Error recovery constants
    constexpr UINT32 MAX_CONSECUTIVE_ERRORS = 3;
    constexpr DWORD RECOVERY_DELAY_MS = 100;
}