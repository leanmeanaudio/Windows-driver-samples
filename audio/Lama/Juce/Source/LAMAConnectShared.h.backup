#pragma once

/* 
 * LAMAConnectShared.h
 * Shared definitions between kernel driver and user-mode applications
 * Must work in both kernel and user mode compilation
 */

#if !defined(_KERNEL_MODE)
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

/* Audio driver constants */
#define LAMA_CONNECT_MAX_CHANNELS    16

/* Base names for shared memory and event objects */
#define LAMA_CONNECT_SHARED_MEMORY_NAME   L"LAMAConnectSharedMemory"
#define LAMA_CONNECT_COMPLETION_EVENT_NAME L"LAMAConnectCompletionEvent"

/* Buffer state enumeration for synchronization */
typedef enum _BUFFER_STATE {
    BUFFER_STATE_EMPTY = 0,
    BUFFER_STATE_READY = 1,
    BUFFER_STATE_PROCESSING = 2
} BUFFER_STATE;

/* Main shared buffer structure */
#pragma pack(push, 1)
typedef struct _LAMA_CONNECT_SHARED_BUFFER {
    /* Audio format - driver always uses 16 channels, 32-bit float */
    UINT32    ChannelCount;         /* Always 16 for driver */
    UINT32    SampleRate;           /* Current sample rate */
    UINT32    BufferSize;           /* Frames per buffer */

    /* Synchronization state */
    volatile BUFFER_STATE BufferState;
    volatile BOOLEAN      IsActive;
    
    /* Statistics/diagnostics */
    UINT64    ProcessedFrames;      /* Total frames processed */
    UINT32    LastProcessingTimeUs; /* Last processing time in microseconds */

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
} LAMA_CONNECT_SHARED_BUFFER, *PLAMA_CONNECT_SHARED_BUFFER;
#pragma pack(pop)

/* IOCTL control codes for driver communication */
#define IOCTL_LAMA_CONNECT_REGISTER          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_UNREGISTER        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_SET_FORMAT        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_LAMA_CONNECT_TRIGGER_PROCESSING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Format structure for SET_FORMAT IOCTL */
typedef struct _LAMA_CONNECT_FORMAT {
    UINT32 SampleRate;    /* Desired sample rate */
    UINT32 ChannelCount;  /* Plugin's active channel count (driver ignores this) */
    UINT32 BufferSize;    /* Frames per buffer */
} LAMA_CONNECT_FORMAT, *PLAMA_CONNECT_FORMAT;