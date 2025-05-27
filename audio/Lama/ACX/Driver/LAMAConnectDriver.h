#ifndef LAMA_CONNECT_DRIVER_H
#define LAMA_CONNECT_DRIVER_H

/*
 * LAMAConnectDriver.h
 * Production-ready ACX virtual audio driver header
 * 
 * Version: 1.1.0 - Production Release
 * Compatible with: Windows 10 19041+ / Windows 11
 * ACX Version: 1.1
 * WDK Version: 10.0.26100.3323+
 */

/* Ensure this is compiled as C, not C++ */
#ifdef __cplusplus
#error "This file must be compiled as C, not C++"
#endif

#define DRIVER_TAG 'AMAL'

/* Ensure kernel mode is defined */
#ifndef _KERNEL_MODE_
#define _KERNEL_MODE_ 1
#endif

#ifndef KERNEL_MODE
#define KERNEL_MODE 1
#endif

/*
 * =============================================================================
 * CORE KERNEL HEADERS
 * =============================================================================
 */

/* Core kernel headers */
#include <ntddk.h>
#include <wdf.h>
#include <wdfio.h>
#include <ntstrsafe.h>
#include <devpkey.h>
#include <wdmguid.h>
#include <devpropdef.h>

/* Security and ETW headers */
#ifdef LAMA_CONNECT_ENABLE_ETW
#include <evntrace.h>
#include <evntprov.h>
#endif

/* Define basic types we need */
#ifndef WORD
typedef unsigned short WORD;
#endif
#ifndef DWORD  
typedef unsigned long DWORD;
#endif

/*
 * =============================================================================
 * AUDIO FORMAT STRUCTURES
 * =============================================================================
 */

#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

typedef struct _WAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX;

typedef struct _WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;

/* KSDATAFORMAT definition */
typedef struct _KSDATAFORMAT {
    ULONG   FormatSize;
    ULONG   Flags;
    ULONG   SampleSize;
    ULONG   Reserved;
    GUID    MajorFormat;
    GUID    SubFormat;
    GUID    Specifier;
} KSDATAFORMAT, *PKSDATAFORMAT;

typedef struct _KSDATAFORMAT_WAVEFORMATEXTENSIBLE {
    KSDATAFORMAT         DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;

/*
 * =============================================================================
 * ACX HEADERS WITH VERSION CHECKING
 * =============================================================================
 */

/* Ensure ACX version is defined */
#ifndef ACX_VERSION_MAJOR
#define ACX_VERSION_MAJOR 1
#endif
#ifndef ACX_VERSION_MINOR
#define ACX_VERSION_MINOR 1
#endif

/* Include essential ACX headers */
#include <acx.h>
#include <acxcircuit.h>
#include <acxpin.h>
#include <acxdataformat.h>
#include <acxstreams.h>
#include <acxtargets.h>

/* Verify ACX version compatibility */
#if (ACX_VERSION_MAJOR < 1) || (ACX_VERSION_MAJOR == 1 && ACX_VERSION_MINOR < 1)
#error "ACX version 1.1 or later is required for production build"
#endif

/* Include our shared definitions */
#include "LAMAConnectShared.h"

/*
 * =============================================================================
 * AUDIO GUIDS
 * =============================================================================
 */

/* Required GUIDs */
DEFINE_GUID(KSDATAFORMAT_TYPE_AUDIO, 
    0x73647561L, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
    0x00000003L, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX,
    0x05589f81L, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

DEFINE_GUID(KSPROPSETID_Pin,
    0x8C134960L, 0x51AD, 0x11CF, 0x87, 0x8A, 0x94, 0xF8, 0x01, 0xC1, 0x00, 0x00);

DEFINE_GUID(KSPIN_CATEGORY_AUDIO,
    0xFBF6F530L, 0x07B9, 0x11D2, 0xA7, 0x1E, 0x00, 0x00, 0xF8, 0x00, 0x47, 0x88);

/* ETW Provider GUID */
#ifdef LAMA_CONNECT_ENABLE_ETW
DEFINE_GUID(LAMA_CONNECT_ETW_PROVIDER,
    0x12345678, 0x1234, 0x5678, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34);
#endif

/*
 * =============================================================================
 * AUDIO CONSTANTS
 * =============================================================================
 */

/* Speaker definitions */
#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define KSAUDIO_SPEAKER_STEREO (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)

/* Production limits and constants */
#define MAX_BUFFER_FRAMES              8192
#define MIN_BUFFER_FRAMES              32
#define DEFAULT_BUFFER_FRAMES          480
#define DEFAULT_SAMPLE_RATE            48000

/*
 * =============================================================================
 * CONTEXT STRUCTURES
 * =============================================================================
 */

/* Circuit context for render vs capture */
typedef struct _CIRCUIT_CONTEXT {
    UINT32                      Magic;              /* LAMA_CIRCUIT_CONTEXT_MAGIC */
    BOOLEAN                     IsRender;
    UINT32                      Reserved[3];        /* Alignment and future use */
} CIRCUIT_CONTEXT, *PCIRCUIT_CONTEXT;

#define LAMA_CIRCUIT_CONTEXT_MAGIC  0x43495243  /* 'CIRC' */

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CIRCUIT_CONTEXT, GetCircuitContext);

/* Power management state */
typedef struct _LAMA_POWER_STATE {
    BOOLEAN                     WasProcessingActive;
    UINT32                      SavedSampleRate;
    UINT32                      SavedBufferSize;
    UINT32                      SavedChannelCount;
    DEVICE_POWER_STATE          LastPowerState;
    LARGE_INTEGER               PowerTransitionTime;
    UINT32                      Reserved[2];        /* Future expansion */
} LAMA_POWER_STATE, *PLAMA_POWER_STATE;

/* Performance statistics */
typedef struct _LAMA_PERFORMANCE_STATS {
    UINT64                      TotalPacketsProcessed;
    UINT64                      TotalBytesProcessed;
    UINT64                      TotalProcessingTimeUs;
    UINT32                      AverageLatencyUs;
    UINT32                      PeakLatencyUs;
    UINT32                      ErrorCount;
    UINT32                      OverrunCount;
    UINT32                      UnderrunCount;
    LARGE_INTEGER               LastResetTime;
    UINT32                      Reserved[4];        /* Future expansion */
} LAMA_PERFORMANCE_STATS, *PLAMA_PERFORMANCE_STATS;

/*
 * =============================================================================
 * DEVICE CONTEXT STRUCTURE (PRODUCTION READY)
 * =============================================================================
 */

typedef struct _LAMA_DEVICE_CONTEXT {
    /* Header with validation */
    UINT32                      Magic;              /* Must be LAMA_DEVICE_CONTEXT_MAGIC */
    UINT32                      Version;            /* Structure version */
    SIZE_T                      StructSize;         /* Size of this structure */
    UINT32                      InstanceIndex;      /* Driver instance (0-3) */
    
    /* WDF device handle */
    WDFDEVICE                   WdfDevice;
    
    /* Shared memory for plugin communication */
    PVOID                       SharedSectionMem;
    PLAMA_CONNECT_SHARED_BUFFER SharedBuffer;
    HANDLE                      SectionHandle;
    SIZE_T                      SharedSectionSize;
    
    /* Completion event for plugin synchronization */
    PKEVENT                     CompletionEventObject;
    HANDLE                      CompletionEventHandle;
    
    /* Plugin connection state */
    volatile BOOLEAN            PluginConnected;
    volatile BOOLEAN            ProcessingActive;
    volatile BOOLEAN            ErrorState;
    UINT8                       Reserved1;          /* Alignment */
    
    /* Synchronization */
    WDFSPINLOCK                 BufferLock;
    WDFTIMER                    Timer;
    WDFWAITLOCK                 ConfigLock;         /* For configuration changes */
    
    /* ACX objects */
    ACXCIRCUIT                  RenderCircuit;
    ACXCIRCUIT                  CaptureCircuit;
    ACXPIN                      RenderPin;
    ACXPIN                      CapturePin;
    ACXSTREAM                   RenderStream;
    ACXSTREAM                   CaptureStream;
    
    /* Packet buffers (double buffered) */
    WDFMEMORY                   RenderMem1;
    WDFMEMORY                   RenderMem2;
    WDFMEMORY                   CaptureMem1;
    WDFMEMORY                   CaptureMem2;
    PVOID                       RenderBuffer1VA;
    PVOID                       RenderBuffer2VA;
    PVOID                       CaptureBuffer1VA;
    PVOID                       CaptureBuffer2VA;
    
    /* Stream state */
    ULONG                       NextCaptureAcxBufferIndex;
    ULONGLONG                   CapturePacketIdSequence;
    ULONGLONG                   LastNotifiedCapturePacketIdValue;
    LARGE_INTEGER               LastNotifiedCaptureQPCValue;
    
    /* Current format info */
    ULONG                       CurrentAcxPacketSizeBytes;
    ULONG                       CurrentAcxFramesPerPacket;
    UINT32                      CurrentAdvertisedOsSampleRate;
    
    /* Power management */
    LAMA_POWER_STATE            PowerState;
    
    /* Performance monitoring */
    LAMA_PERFORMANCE_STATS      PerfStats;
    
    /* Security and validation */
    LARGE_INTEGER               CreationTime;
    UINT32                      ValidationChecksum;
    
    /* ETW logging */
#ifdef LAMA_CONNECT_ENABLE_ETW
    REGHANDLE                   ETWHandle;
#endif
    
    /* Reserved for future expansion */
    UINT64                      Reserved2[8];
    
} LAMA_DEVICE_CONTEXT, *PLAMA_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(LAMA_DEVICE_CONTEXT, LAMAGetDeviceContext);

/*
 * =============================================================================
 * FUNCTION DECLARATIONS
 * =============================================================================
 */

/* Driver callbacks */
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD LAMAEvtDeviceAdd;
EVT_WDF_DRIVER_CONTEXT_CLEANUP LAMAEvtDriverContextCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL LAMAEvtIoDeviceControl;
EVT_WDF_DEVICE_CONTEXT_CLEANUP LAMA_DeviceContextCleanup;

/* Power management callbacks */
EVT_WDF_DEVICE_D0_ENTRY LAMAEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT LAMAEvtDeviceD0Exit;
EVT_WDF_DEVICE_PREPARE_HARDWARE LAMAEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE LAMAEvtDeviceReleaseHardware;

/* ACX device callbacks */
EVT_ACX_DEVICE_CREATE_STREAM LAMAEvtDeviceCreateStream;

/* ACX stream callbacks */
EVT_ACX_STREAM_PREPARE_HARDWARE LAMA_EvtStreamPrepareHardware;
EVT_ACX_STREAM_RELEASE_HARDWARE LAMA_EvtStreamReleaseHardware;
EVT_ACX_STREAM_RUN LAMA_EvtStreamRun;
EVT_ACX_STREAM_PAUSE LAMA_EvtStreamPause;

/* ACX RT packet callbacks */
EVT_ACX_STREAM_ALLOCATE_RTPACKETS LAMA_EvtStreamAllocateRtPackets;
EVT_ACX_STREAM_FREE_RTPACKETS LAMA_EvtStreamFreeRtPackets;
EVT_ACX_STREAM_GET_CAPTURE_PACKET LAMA_EvtStreamGetCapturePacket;
EVT_ACX_STREAM_SET_RENDER_PACKET LAMA_EvtStreamSetRenderPacket;

/* Timer callback */
EVT_WDF_TIMER LAMA_TimerTick;

/*
 * =============================================================================
 * CORE HELPER FUNCTIONS
 * =============================================================================
 */

/* Audio conversion functions */
VOID ConvertPlanarToInterleavedMaxCh(
    _In_reads_(frames * channels) float* planarSource,
    _Out_writes_(frames * LAMA_CONNECT_MAX_CHANNELS) float* interleavedDest,
    _In_ UINT32 frames,
    _In_ UINT32 channels);

VOID ConvertInterleavedMaxChToPlanar(
    _In_reads_(frames * LAMA_CONNECT_MAX_CHANNELS) float* interleavedSource,
    _Out_writes_(frames * channels) float* planarDest,
    _In_ UINT32 frames,
    _In_ UINT32 channels);

/* Memory and resource management */
NTSTATUS CreateSharedMemorySection(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

NTSTATUS CreateCompletionEvent(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

NTSTATUS LAMA_AllocateStreamPacketBuffer(
    _In_ PLAMA_DEVICE_CONTEXT DevContext,
    _In_ SIZE_T BufferSizeInBytes,
    _Out_ WDFMEMORY* MemoryHandle);

VOID LAMA_CleanupSharedMemory(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

/* ACX initialization */
NTSTATUS InitializeACXDevice(
    _In_ WDFDEVICE Device,
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

NTSTATUS RegisterCircuitsWithACX(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

/* Stream management */
BOOLEAN LAMA_IsRenderStream(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext,
    _In_ ACXSTREAM Stream);

NTSTATUS UpdateAcxPinAdvertisedSampleRate(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext, 
    _In_ UINT32 newSampleRate);

/*
 * =============================================================================
 * VALIDATION AND SECURITY FUNCTIONS
 * =============================================================================
 */

/* Context validation */
NTSTATUS ValidateDeviceContext(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

NTSTATUS ValidateCircuitContext(
    _In_ PCIRCUIT_CONTEXT CircuitContext);

/* Parameter validation */
NTSTATUS ValidateAudioFormat(
    _In_ PLAMA_CONNECT_FORMAT* Format);

NTSTATUS ValidateBufferParameters(
    _In_ UINT32 SampleRate,
    _In_ UINT32 BufferSize,
    _In_ UINT32 ChannelCount);

/* Security functions */
NTSTATUS SecureZeroDeviceContext(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

UINT32 CalculateDeviceContextChecksum(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

BOOLEAN VerifyDeviceContextIntegrity(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

/*
 * =============================================================================
 * PERFORMANCE AND MONITORING
 * =============================================================================
 */

/* Performance monitoring */
VOID UpdatePerformanceStats(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext,
    _In_ UINT32 ProcessingTimeUs,
    _In_ UINT32 FramesProcessed);

VOID ResetPerformanceStats(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

/*
 * =============================================================================
 * ETW LOGGING SUPPORT
 * =============================================================================
 */

#ifdef LAMA_CONNECT_ENABLE_ETW

/* ETW levels */
#define LAMA_ETW_LEVEL_ERROR        1
#define LAMA_ETW_LEVEL_WARNING      2
#define LAMA_ETW_LEVEL_INFO         3
#define LAMA_ETW_LEVEL_VERBOSE      4

/* ETW logging functions */
NTSTATUS InitializeETWLogging(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

VOID CleanupETWLogging(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext);

VOID LAMAETWLogEvent(
    _In_ PLAMA_DEVICE_CONTEXT DeviceContext,
    _In_ UCHAR Level,
    _In_ PCWSTR Format,
    ...);

/* ETW logging macros */
#define LAMA_LOG_ERROR(ctx, format, ...) \
    LAMAETWLogEvent((ctx), LAMA_ETW_LEVEL_ERROR, L"[ERROR] " format, __VA_ARGS__)

#define LAMA_LOG_WARNING(ctx, format, ...) \
    LAMAETWLogEvent((ctx), LAMA_ETW_LEVEL_WARNING, L"[WARNING] " format, __VA_ARGS__)

#define LAMA_LOG_INFO(ctx, format, ...) \
    LAMAETWLogEvent((ctx), LAMA_ETW_LEVEL_INFO, L"[INFO] " format, __VA_ARGS__)

#else

/* Stub macros when ETW is disabled */
#define LAMA_LOG_ERROR(ctx, format, ...) ((void)0)
#define LAMA_LOG_WARNING(ctx, format, ...) ((void)0)
#define LAMA_LOG_INFO(ctx, format, ...) ((void)0)

#endif /* LAMA_CONNECT_ENABLE_ETW */

/*
 * =============================================================================
 * DEBUG SUPPORT
 * =============================================================================
 */

#ifdef LAMA_CONNECT_DEBUG

#define LAMA_TRACE(format, ...) \
    DbgPrint("[LAMA:%s:%d] " format "\n", __FUNCTION__, __LINE__, __VA_ARGS__)

#define LAMA_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            DbgPrint("[LAMA] ASSERTION FAILED: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            DbgBreakPoint(); \
        } \
    } while(0)

#define LAMA_VERIFY_SUCCESS(status) \
    do { \
        NTSTATUS _status = (status); \
        if (!NT_SUCCESS(_status)) { \
            DbgPrint("[LAMA] OPERATION FAILED: 0x%X at %s:%d\n", _status, __FILE__, __LINE__); \
        } \
    } while(0)

#else

#define LAMA_TRACE(format, ...) ((void)0)
#define LAMA_ASSERT(condition) ((void)0)
#define LAMA_VERIFY_SUCCESS(status) ((void)0)

#endif /* LAMA_CONNECT_DEBUG */

/*
 * =============================================================================
 * PRODUCTION SAFETY MACROS
 * =============================================================================
 */

/* Safe pointer validation */
#define LAMA_VALIDATE_POINTER(ptr, status) \
    do { \
        if ((ptr) == NULL) { \
            LAMA_LOG_ERROR(NULL, L"NULL pointer validation failed"); \
            return (status); \
        } \
    } while(0)

/* Safe context validation */
#define LAMA_VALIDATE_DEVICE_CONTEXT(ctx) \
    do { \
        NTSTATUS _status = ValidateDeviceContext(ctx); \
        if (!NT_SUCCESS(_status)) { \
            LAMA_LOG_ERROR((ctx), L"Device context validation failed: 0x%X", _status); \
            return _status; \
        } \
    } while(0)

/* Safe memory operations */
#define LAMA_SAFE_ZERO_MEMORY(ptr, size) \
    do { \
        if ((ptr) != NULL && (size) > 0) { \
            RtlSecureZeroMemory((ptr), (size)); \
        } \
    } while(0)

/*
 * =============================================================================
 * COMPILER AND ARCHITECTURE SUPPORT
 * =============================================================================
 */

/* Compiler-specific optimizations */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201)  /* nonstandard extension used: nameless struct/union */
#pragma warning(disable: 4214)  /* nonstandard extension used: bit field types other than int */
#endif

/* Architecture-specific definitions */
#if defined(_M_X64) || defined(_M_AMD64)
#define LAMA_ARCH_X64 1
#elif defined(_M_ARM64)
#define LAMA_ARCH_ARM64 1
#else
#error "Unsupported architecture"
#endif

/* Memory barriers for different architectures */
#if defined(LAMA_ARCH_ARM64)
#define LAMA_MEMORY_BARRIER() __dmb(_ARM64_BARRIER_SY)
#else
#define LAMA_MEMORY_BARRIER() MemoryBarrier()
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* LAMA_CONNECT_DRIVER_H */