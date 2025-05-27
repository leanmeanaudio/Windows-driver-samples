#pragma once

/* 
 * LAMAConnectShared.h
 * Shared definitions between kernel driver and user-mode applications
 * Production-ready version with comprehensive security and validation
 * 
 * Version: 1.1.0 - Production Release
 * Compatible with: Windows 10 19041+ / Windows 11
 * ACX Version: 1.1
 * WDK Version: 10.0.26100.3323+
 */

#if !defined(_KERNEL_MODE) && !defined(_KERNEL_MODE_)
/* User-mode includes */
#include <windows.h>
#include <winioctl.h>

/* Ensure we have the basic types */
#ifndef UINT32
typedef unsigned int UINT32;
#endif
#ifndef UINT64
typedef unsigned __int64 UINT64;
#endif
#ifndef BOOLEAN
typedef unsigned char BOOLEAN;
#endif

#else
/* Kernel mode - basic types provided by ntddk.h, define what we need */
#ifndef UINT32
typedef unsigned int UINT32;
#endif
#ifndef UINT64
typedef unsigned __int64 UINT64;
#endif
#ifndef BOOLEAN
typedef unsigned char BOOLEAN;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef HANDLE
typedef PVOID HANDLE;
#endif
#endif

/* Define IOCTL constants if not available */
#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif
#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif
#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0
#endif
#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

/*
 * =============================================================================
 * PRODUCTION SECURITY CONSTANTS
 * =============================================================================
 */

/* Magic numbers for corruption detection */
#define LAMA_SHARED_BUFFER_MAGIC        0x53484152  /* 'SHAR' */
#define LAMA_DEVICE_CONTEXT_MAGIC       0x4C414D41  /* 'LAMA' */

/* Version information */
#define LAMA_CONNECT_VERSION_MAJOR       1
#define LAMA_CONNECT_VERSION_MINOR       1
#define LAMA_CONNECT_VERSION_BUILD       0
#define LAMA_CONNECT_VERSION_REVISION    0

#define LAMA_SHARED_BUFFER_VERSION      0x00010001  /* Version 1.1 */

/* Audio driver constants */
#define LAMA_CONNECT_MAX_CHANNELS        16
#define LAMA_CONNECT_MAX_INSTANCES       4

/* Security and validation constants */
#define LAMA_CONNECT_MIN_SAMPLE_RATE     8000
#define LAMA_CONNECT_MAX_SAMPLE_RATE     192000
#define LAMA_CONNECT_MIN_BUFFER_SIZE     32
#define LAMA_CONNECT_MAX_BUFFER_SIZE     8192
#define LAMA_CONNECT_MIN_CHANNELS        1

/* Maximum allowed buffer sizes for security */
#define MAX_ALLOWED_BUFFER_SIZE          (8192 * 16 * sizeof(float))  /* 8K frames, 16 ch, float */
#define MAX_SHARED_MEMORY_SIZE           (1024 * 1024)  /* 1MB maximum */

/* Base names for shared memory and event objects */
#define LAMA_CONNECT_SHARED_MEMORY_NAME   L"LAMAConnectSharedMemory"
#define LAMA_CONNECT_COMPLETION_EVENT_NAME L"LAMAConnectCompletionEvent"

/*
 * =============================================================================
 * DATA STRUCTURES
 * =============================================================================
 */

/* Buffer state enumeration for synchronization */
typedef enum _BUFFER_STATE {
    BUFFER_STATE_EMPTY = 0,
    BUFFER_STATE_READY = 1,
    BUFFER_STATE_PROCESSING = 2,
    BUFFER_STATE_ERROR = 3,
    BUFFER_STATE_MAX = 4  /* For validation */
} BUFFER_STATE;

/* Production-ready shared buffer structure */
#pragma pack(push, 1)
typedef struct _LAMA_CONNECT_SHARED_BUFFER {
    /* Header with validation */
    UINT32    Magic;                    /* Must be LAMA_SHARED_BUFFER_MAGIC */
    UINT32    Version;                  /* Must be LAMA_SHARED_BUFFER_VERSION */
    UINT32    StructSize;               /* Size of this structure */
    UINT32    Reserved1;                /* Alignment padding */

    /* Audio format - driver always uses 16 channels, 32-bit float */
    UINT32    ChannelCount;             /* Always 16 for driver */
    UINT32    SampleRate;               /* Current sample rate */
    UINT32    BufferSize;               /* Frames per buffer */
    UINT32    BytesPerFrame;            /* Bytes per frame (channels * sizeof(float)) */

    /* Synchronization state */
    volatile BUFFER_STATE BufferState;
    volatile BOOLEAN      IsActive;
    volatile BOOLEAN      ErrorOccurred;
    UINT8                 Reserved2;    /* Alignment padding */
    
    /* Statistics and diagnostics */
    volatile UINT64   ProcessedFrames;      /* Total frames processed */
    volatile UINT32   LastProcessingTimeUs; /* Last processing time in microseconds */
    volatile UINT32   ErrorCount;           /* Number of processing errors */
    volatile UINT32   OverrunCount;         /* Buffer overrun count */
    volatile UINT32   UnderrunCount;        /* Buffer underrun count */

    /* Audio buffer offsets (from start of shared memory section) */
    UINT32    PluginToDriverBufferOffset;   /* Plugin input -> Driver (planar) */
    UINT32    DriverToPluginBufferOffset;   /* Driver output -> Plugin (planar) */
    UINT32    AppAudioOutputBufferOffset;   /* Driver -> Apps via capture endpoint */
    UINT32    AppAudioInputBufferOffset;    /* Apps -> Driver via render endpoint */
    
    /* Buffer sizes in bytes */
    UINT32    PluginToDriverBufferSize;
    UINT32    DriverToPluginBufferSize;
    UINT32    AppAudioOutputBufferSize;
    UINT32    AppAudioInputBufferSize;
    
    /* Completion event handle (NULL in kernel, opened by name in user mode) */
    HANDLE    CompletionEvent;
    
    /* Performance monitoring */
    UINT64    LastUpdateTimestamp;      /* QPC timestamp of last update */
    UINT32    AverageProcessingTimeUs;  /* Moving average of processing time */
    UINT32    PeakProcessingTimeUs;     /* Peak processing time recorded */
    
    /* Security and validation */
    UINT32    ValidationChecksum;       /* Simple checksum for structure validation */
    UINT32    Reserved3[8];             /* Future expansion, must be zero */
} LAMA_CONNECT_SHARED_BUFFER, *PLAMA_CONNECT_SHARED_BUFFER;
#pragma pack(pop)

/* Format structure for SET_FORMAT IOCTL */
typedef struct _LAMA_CONNECT_FORMAT {
    UINT32 Magic;           /* Must be LAMA_CONNECT_FORMAT_MAGIC for validation */
    UINT32 SampleRate;      /* Desired sample rate */
    UINT32 ChannelCount;    /* Plugin's active channel count (driver uses this for internal processing) */
    UINT32 BufferSize;      /* Frames per buffer */
    UINT32 Reserved[4];     /* Future expansion, must be zero */
} LAMA_CONNECT_FORMAT, *PLAMA_CONNECT_FORMAT;

#define LAMA_CONNECT_FORMAT_MAGIC   0x464D5441  /* 'ATFM' - Audio Format */

/*
 * =============================================================================
 * IOCTL DEFINITIONS
 * =============================================================================
 */

/* IOCTL control codes for driver communication */
#define IOCTL_LAMA_CONNECT_REGISTER          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_UNREGISTER        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_SET_FORMAT        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_TRIGGER_PROCESSING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_GET_STATUS        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_RESET_STATS       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
 * =============================================================================
 * ERROR CODES
 * =============================================================================
 */

/* Custom error codes (in addition to standard Windows error codes) */
#define LAMA_ERROR_DRIVER_NOT_FOUND      0xE0000001L
#define LAMA_ERROR_INVALID_FORMAT        0xE0000002L
#define LAMA_ERROR_BUFFER_OVERFLOW       0xE0000003L
#define LAMA_ERROR_PROCESSING_TIMEOUT    0xE0000004L
#define LAMA_ERROR_INSTANCE_UNAVAILABLE  0xE0000005L
#define LAMA_ERROR_MAGIC_MISMATCH        0xE0000006L
#define LAMA_ERROR_VERSION_MISMATCH      0xE0000007L
#define LAMA_ERROR_CORRUPTED_DATA        0xE0000008L
#define LAMA_ERROR_SECURITY_VIOLATION    0xE0000009L

/*
 * =============================================================================
 * VALIDATION MACROS
 * =============================================================================
 */

/* Sample rate validation */
#define LAMA_IS_VALID_SAMPLE_RATE(rate) \
    ((rate) >= LAMA_CONNECT_MIN_SAMPLE_RATE && (rate) <= LAMA_CONNECT_MAX_SAMPLE_RATE)

/* Buffer size validation */
#define LAMA_IS_VALID_BUFFER_SIZE(size) \
    ((size) >= LAMA_CONNECT_MIN_BUFFER_SIZE && (size) <= LAMA_CONNECT_MAX_BUFFER_SIZE)

/* Channel count validation */
#define LAMA_IS_VALID_CHANNEL_COUNT(count) \
    ((count) >= LAMA_CONNECT_MIN_CHANNELS && (count) <= LAMA_CONNECT_MAX_CHANNELS)

/* Instance index validation */
#define LAMA_IS_VALID_INSTANCE_INDEX(index) \
    ((index) >= 0 && (index) < LAMA_CONNECT_MAX_INSTANCES)

/* Buffer state validation */
#define LAMA_IS_VALID_BUFFER_STATE(state) \
    ((state) >= BUFFER_STATE_EMPTY && (state) < BUFFER_STATE_MAX)

/* Magic number validation */
#define LAMA_IS_VALID_SHARED_BUFFER_MAGIC(magic) \
    ((magic) == LAMA_SHARED_BUFFER_MAGIC)

#define LAMA_IS_VALID_FORMAT_MAGIC(magic) \
    ((magic) == LAMA_CONNECT_FORMAT_MAGIC)

/* Buffer offset validation */
#define LAMA_VALIDATE_BUFFER_OFFSET(offset, maxSize) \
    ((offset) < (maxSize) && ((offset) % 16) == 0) /* 16-byte alignment */

/* Size validation with overflow protection */
#define LAMA_IS_SAFE_SIZE_MULTIPLY(a, b) \
    ((a) == 0 || (b) == 0 || (a) <= (SIZE_MAX / (b)))

#define LAMA_IS_SAFE_SIZE_ADD(a, b) \
    ((a) <= (SIZE_MAX - (b)))

/*
 * =============================================================================
 * UTILITY MACROS
 * =============================================================================
 */

/* Convert frames to bytes */
#define LAMA_FRAMES_TO_BYTES(frames, channels) \
    ((frames) * (channels) * sizeof(float))

/* Convert bytes to frames */
#define LAMA_BYTES_TO_FRAMES(bytes, channels) \
    ((bytes) / ((channels) * sizeof(float)))

/* Calculate total buffer size needed */
#define LAMA_CALCULATE_TOTAL_BUFFER_SIZE(frames, channels) \
    (sizeof(LAMA_CONNECT_SHARED_BUFFER) + (LAMA_FRAMES_TO_BYTES(frames, channels) * 4))

/* Align size to page boundary */
#define LAMA_ALIGN_TO_PAGE(size) \
    (((size) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Simple checksum calculation for validation */
#define LAMA_CALCULATE_SIMPLE_CHECKSUM(ptr, size) \
    LAMACalculateSimpleChecksum((const UINT8*)(ptr), (size))

/*
 * =============================================================================
 * INLINE UTILITY FUNCTIONS
 * =============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Simple checksum function for structure validation */
static __inline UINT32 LAMACalculateSimpleChecksum(const UINT8* data, SIZE_T size) {
    UINT32 checksum = 0;
    SIZE_T i;
    
    if (!data || size == 0) {
        return 0;
    }
    
    for (i = 0; i < size; ++i) {
        checksum = (checksum << 1) ^ data[i];
    }
    
    return checksum;
}

/* Validate shared buffer structure */
static __inline BOOLEAN LAMAIsValidSharedBuffer(const LAMA_CONNECT_SHARED_BUFFER* buffer) {
    if (!buffer) {
        return FALSE;
    }
    
    /* Check magic number */
    if (!LAMA_IS_VALID_SHARED_BUFFER_MAGIC(buffer->Magic)) {
        return FALSE;
    }
    
    /* Check version */
    if (buffer->Version != LAMA_SHARED_BUFFER_VERSION) {
        return FALSE;
    }
    
    /* Check structure size */
    if (buffer->StructSize != sizeof(LAMA_CONNECT_SHARED_BUFFER)) {
        return FALSE;
    }
    
    /* Check basic audio parameters */
    if (!LAMA_IS_VALID_SAMPLE_RATE(buffer->SampleRate) ||
        !LAMA_IS_VALID_BUFFER_SIZE(buffer->BufferSize) ||
        !LAMA_IS_VALID_CHANNEL_COUNT(buffer->ChannelCount)) {
        return FALSE;
    }
    
    /* Check buffer state */
    if (!LAMA_IS_VALID_BUFFER_STATE(buffer->BufferState)) {
        return FALSE;
    }
    
    return TRUE;
}

/* Validate format structure */
static __inline BOOLEAN LAMAIsValidFormat(const LAMA_CONNECT_FORMAT* format) {
    if (!format) {
        return FALSE;
    }
    
    /* Check magic number */
    if (!LAMA_IS_VALID_FORMAT_MAGIC(format->Magic)) {
        return FALSE;
    }
    
    /* Check audio parameters */
    if (!LAMA_IS_VALID_SAMPLE_RATE(format->SampleRate) ||
        !LAMA_IS_VALID_BUFFER_SIZE(format->BufferSize) ||
        !LAMA_IS_VALID_CHANNEL_COUNT(format->ChannelCount)) {
        return FALSE;
    }
    
    return TRUE;
}

#ifdef __cplusplus
}
#endif

/*
 * =============================================================================
 * DEBUG AND LOGGING MACROS
 * =============================================================================
 */

#if defined(LAMA_CONNECT_DEBUG) || defined(_DEBUG) || defined(DBG)
#define LAMA_DEBUG_PRINT(format, ...) \
    DbgPrint("[LAMA] " format "\n", __VA_ARGS__)
    
#define LAMA_ASSERT(condition) \
    do { if (!(condition)) { \
        DbgPrint("[LAMA] ASSERTION FAILED: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        DbgBreakPoint(); \
    } } while(0)
#else
#define LAMA_DEBUG_PRINT(format, ...) ((void)0)
#define LAMA_ASSERT(condition) ((void)0)
#endif

/* ETW logging support (when enabled) */
#ifdef LAMA_CONNECT_ENABLE_ETW
#define LAMA_ETW_LOG_ERROR(format, ...) \
    LAMAETWLogEvent(LAMA_ETW_LEVEL_ERROR, format, __VA_ARGS__)
    
#define LAMA_ETW_LOG_WARNING(format, ...) \
    LAMAETWLogEvent(LAMA_ETW_LEVEL_WARNING, format, __VA_ARGS__)
    
#define LAMA_ETW_LOG_INFO(format, ...) \
    LAMAETWLogEvent(LAMA_ETW_LEVEL_INFO, format, __VA_ARGS__)
#else
#define LAMA_ETW_LOG_ERROR(format, ...) ((void)0)
#define LAMA_ETW_LOG_WARNING(format, ...) ((void)0)
#define LAMA_ETW_LOG_INFO(format, ...) ((void)0)
#endif

/*
 * =============================================================================
 * SECURITY ANNOTATIONS
 * =============================================================================
 */

/* SAL annotations for better static analysis */
#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _In_reads_
#define _In_reads_(size)
#endif
#ifndef _Out_writes_
#define _Out_writes_(size)
#endif

/*
 * =============================================================================
 * COMPATIBILITY AND FEATURE DETECTION
 * =============================================================================
 */

/* Feature flags for different driver capabilities */
#define LAMA_FEATURE_MULTI_INSTANCE     0x00000001
#define LAMA_FEATURE_HIGH_SAMPLE_RATES  0x00000002
#define LAMA_FEATURE_LOW_LATENCY        0x00000004
#define LAMA_FEATURE_POWER_MANAGEMENT   0x00000008
#define LAMA_FEATURE_ETW_LOGGING        0x00000010
#define LAMA_FEATURE_ARM64_SUPPORT      0x00000020

/* Current driver capabilities */
#define LAMA_CURRENT_FEATURES ( \
    LAMA_FEATURE_MULTI_INSTANCE | \
    LAMA_FEATURE_HIGH_SAMPLE_RATES | \
    LAMA_FEATURE_LOW_LATENCY | \
    LAMA_FEATURE_POWER_MANAGEMENT | \
    LAMA_FEATURE_ETW_LOGGING | \
    LAMA_FEATURE_ARM64_SUPPORT \
)

/* End of file marker */
#define LAMA_CONNECT_SHARED_H_INCLUDED 1