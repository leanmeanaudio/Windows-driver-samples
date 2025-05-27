#include "waveformat_fix.h"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility

#pragma once

// Include standard Windows kernel-mode driver headers first
#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>  // Required for DEFINE_GUID to create actual GUIDs

// Include KS headers in the correct order
#include <ks.h>
#include <ksmedia.h>
#include <portcls.h>

// Define KSDATAFORMAT_WAVEFORMATEXTENSIBLE if not already defined
#if !defined(KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED)
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif

// Define GUID for Lama Loopback category
// {9F2A7793-A5EB-4301-8C96-7B45ADC1DCDE}
DEFINE_GUID(KSCATEGORY_LAMA_LOOPBACK, 
    0x9f2a7793, 0xa5eb, 0x4301, 0x8c, 0x96, 0x7b, 0x45, 0xad, 0xc1, 0xdc, 0xde);

// Define GUID for Lama Loopback bridge pin
// {E9DD7D81-9230-4C7F-B08A-31321D5294B7}
DEFINE_GUID(LAMA_LOOPBACK_BRIDGE_PIN_IN, 
    0xe9dd7d81, 0x9230, 0x4c7f, 0xb0, 0x8a, 0x31, 0x32, 0x1d, 0x52, 0x94, 0xb7);

// Define the pin name GUIDs for Lama Loopback wave inputs and outputs
// {2DA3E856-4D9E-4CCD-BC6C-8E2A48A5E700}
DEFINE_GUID(PINNAME_LamaLoopbackWaveIn, 
    0x2da3e856, 0x4d9e, 0x4ccd, 0xbc, 0x6c, 0x8e, 0x2a, 0x48, 0xa5, 0xe7, 0x00);

// {3C7C8E85-0F4C-4071-B17C-3F9E5C9F4B1A}
DEFINE_GUID(PINNAME_LamaLoopbackWaveOut, 
    0x3c7c8e85, 0x0f4c, 0x4071, 0xb1, 0x7c, 0x3f, 0x9e, 0x5c, 0x9f, 0x4b, 0x1a);

// Pool Tag
#define LAMA_POOL_TAG 'BLSL'

// Max driver instances
#define MAX_LAMA_INSTANCES 4
#ifndef LAMA_LOOPBACK_DEVICE_MAX_INSTANCES // Ensure it's defined if not already
#define LAMA_LOOPBACK_DEVICE_MAX_INSTANCES MAX_LAMA_INSTANCES
#endif

// Buffer size constants
#define LAMA_MAX_STREAM_BUFFER_SIZE (1024 * 1024)
#define LAMA_DEFAULT_PACKET_SIZE_MS 10
#define LAMA_SHARED_RING_BUFFER_SIZE (65536 * 2) // This should be large enough for 16ch data

// Pin properties (Existing definitions unchanged)
#ifndef STATIC_KSPIN_WAVE_RENDER_SINK_IN
#define STATIC_KSPIN_WAVE_RENDER_SINK_IN 0
#endif
#ifndef STATIC_KSPIN_WAVE_RENDER_SOURCE_OUT
#define STATIC_KSPIN_WAVE_RENDER_SOURCE_OUT 1
#endif
#ifndef STATIC_KSPIN_WAVE_CAPTURE_SOURCE_IN
#define STATIC_KSPIN_WAVE_CAPTURE_SOURCE_IN 0
#endif
#ifndef STATIC_KSPIN_WAVE_CAPTURE_SINK_OUT
#define STATIC_KSPIN_WAVE_CAPTURE_SINK_OUT 1
#endif
#ifndef STATIC_KSPIN_TOPO_RENDER_BRIDGE_IN
#define STATIC_KSPIN_TOPO_RENDER_BRIDGE_IN 0
#endif
#ifndef STATIC_KSPIN_TOPO_RENDER_LOOPBACK_OUT
#define STATIC_KSPIN_TOPO_RENDER_LOOPBACK_OUT 1
#endif
#ifndef STATIC_KSPIN_TOPO_CAPTURE_LOOPBACK_IN
#define STATIC_KSPIN_TOPO_CAPTURE_LOOPBACK_IN 0
#endif
#ifndef STATIC_KSPIN_TOPO_CAPTURE_BRIDGE_OUT
#define STATIC_KSPIN_TOPO_CAPTURE_BRIDGE_OUT 1
#endif

// Node properties (Existing definitions unchanged)
#ifndef STATIC_KSNODEID_SYSAUDIO_VOLUME
#define STATIC_KSNODEID_SYSAUDIO_VOLUME 0
#endif
#ifndef STATIC_KSNODEID_SYSAUDIO_MUTE
#define STATIC_KSNODEID_SYSAUDIO_MUTE 1
#endif
#define STATIC_KSNODE_TOPO_RENDER_VOLUME    STATIC_KSNODEID_SYSAUDIO_VOLUME
#define STATIC_KSNODE_TOPO_RENDER_MUTE      STATIC_KSNODEID_SYSAUDIO_MUTE
#define STATIC_KSNODE_TOPO_CAPTURE_VOLUME   STATIC_KSNODEID_SYSAUDIO_VOLUME
#define STATIC_KSNODE_TOPO_CAPTURE_MUTE     STATIC_KSNODEID_SYSAUDIO_MUTE

// Data Formats & Ranges (Existing definitions unchanged)
#define MIN_SAMPLE_RATE_PCM       8000
#define MAX_SAMPLE_RATE_PCM       192000
#define DEFAULT_SAMPLE_RATE_PCM   48000
#define MIN_CHANNELS_PCM    2       // Min channels clients can request
#define MAX_CHANNELS_PCM    16      // Max channels clients can request AND internal driver channel count
#define DEFAULT_CHANNELS_PCM 2
#define MIN_BITS_PER_SAMPLE_PCM   16
#define MAX_BITS_PER_SAMPLE_PCM   16      // For simplicity, driver uses 16-bit for shared buffer
#define DEFAULT_BITS_PER_SAMPLE_PCM 16

// Shared Ring Buffer for Loopback (Per Instance)
typedef struct _LAMA_SHARED_LOOPBACK_BUFFER {
    PBYTE           pBuffer;
    ULONG           ulBufferSize;
    volatile ULONG  ulWritePointer;
    volatile ULONG  ulReadPointer;
    KSPIN_LOCK      SpinLock;
    BOOLEAN         bInitialized;
} LAMA_SHARED_LOOPBACK_BUFFER, *PLAMA_SHARED_LOOPBACK_BUFFER;

extern LAMA_SHARED_LOOPBACK_BUFFER g_InstanceLoopbackBuffers[MAX_LAMA_INSTANCES];
NTSTATUS InitializeAllSharedLoopbackBuffers(VOID);
VOID FreeAllSharedLoopbackBuffers(VOID);

// Global Audio Format Variables (Existing definitions unchanged)
extern ULONG g_CurrentGlobalSampleRate;
extern ULONG g_CurrentGlobalChannels; // This might be less relevant if internal is always 16
extern ULONG g_CurrentGlobalBitsPerSample;

// Lama Loopback Sample Rate Control Property Set (Existing definitions unchanged)
DEFINE_GUIDSTRUCT("7C4E6248-4C7D-4FE4-8E4F-4E62484C7D00", KSPROPSETID_LamaLoopback);
#define KSPROPSETID_LamaLoopback DEFINE_GUIDNAMED(KSPROPSETID_LamaLoopback)
typedef enum { KSPROPERTY_LAMA_SAMPLE_RATE = 0 } KSPROPERTY_LAMA;

#define DEFINE_KSPROPERTY_ITEM_LAMA_SAMPLE_RATE(GetHandler, SetHandler) \
    DEFINE_KSPROPERTY_ITEM( \
        KSPROPERTY_LAMA_SAMPLE_RATE, \
        (PFNKSPROPERTYHANDLER)(GetHandler), \
        sizeof(KSPROPERTY), \
        sizeof(ULONG), \
        (PFNKSPROPERTYHANDLER)(SetHandler), \
        NULL, 0, NULL, NULL, 0 \
    )

// Extern declaration for the Automation Table defined in lamaloopbackrender.cpp
extern const KSAUTOMATION_TABLE LamaLoopbackFilterAutomationTable;

// Debugging DPF levels and macros (Existing definitions unchanged)
#ifndef DPF_LEVEL_ERROR
#define DPF_LEVEL_ERROR     DPFLTR_ERROR_LEVEL
#endif
#ifndef DPF_LEVEL_WARNING
#define DPF_LEVEL_WARNING   DPFLTR_WARNING_LEVEL
#endif
#ifndef DPF_LEVEL_INFO
#define DPF_LEVEL_INFO      DPFLTR_INFO_LEVEL
#endif
#ifndef DPF_LEVEL_TRACE
#define DPF_LEVEL_TRACE     DPFLTR_TRACE_LEVEL
#endif
#ifndef DPF_LEVEL_TERSE
#define DPF_LEVEL_TERSE     DPFLTR_TERSE_LEVEL
#endif

#ifndef DPF
  #if DBG
    #define DPF(lvl, _x_) DbgPrint _x_
  #else
    #define DPF(lvl, _x_)
  #endif
#endif
#ifndef DPF_ENTER
  #define DPF_ENTER(func_name) DPF(DPF_LEVEL_TRACE, ("Entered " func_name "\n"))
#endif
#ifndef DPF_LEAVE
  #define DPF_LEAVE(func_name_status) DPF(DPF_LEVEL_TRACE, ("Exiting " func_name_status "\n"))
#endif
// Pin Name GUIDs (Ensure these are defined)
#ifndef PINNAME_LamaLoopbackWaveIn
DEFINE_GUIDSTRUCT("E45D3AAB-1D61-4304-8AF8-A39A5E23A0F7", PINNAME_LamaLoopbackWaveIn);
#define PINNAME_LamaLoopbackWaveIn DEFINE_GUIDNAMED(PINNAME_LamaLoopbackWaveIn)
#endif
#ifndef PINNAME_LamaLoopbackWaveOut
DEFINE_GUIDSTRUCT("F45D3AAB-1D61-4304-8AF8-A39A5E23A0F7", PINNAME_LamaLoopbackWaveOut);
#define PINNAME_LamaLoopbackWaveOut DEFINE_GUIDNAMED(PINNAME_LamaLoopbackWaveOut)
#endif
#ifndef LAMA_LOOPBACK_BRIDGE_PIN_IN
DEFINE_GUIDSTRUCT("045D3AAB-1D61-4304-8AF8-A39A5E23A0F7", LAMA_LOOPBACK_BRIDGE_PIN_IN);
#define LAMA_LOOPBACK_BRIDGE_PIN_IN DEFINE_GUIDNAMED(LAMA_LOOPBACK_BRIDGE_PIN_IN)
#endif
#ifndef LAMA_LOOPBACK_BRIDGE_PIN_OUT
DEFINE_GUIDSTRUCT("145D3AAB-1D61-4304-8AF8-A39A5E23A0F7", LAMA_LOOPBACK_BRIDGE_PIN_OUT);
#define LAMA_LOOPBACK_BRIDGE_PIN_OUT DEFINE_GUIDNAMED(LAMA_LOOPBACK_BRIDGE_PIN_OUT)
#endif
#ifndef KSCATEGORY_LAMA_LOOPBACK
DEFINE_GUIDSTRUCT("245D3AAB-1D61-4304-8AF8-A39A5E23A0F7", KSCATEGORY_LAMA_LOOPBACK);
#define KSCATEGORY_LAMA_LOOPBACK DEFINE_GUIDNAMED(KSCATEGORY_LAMA_LOOPBACK)
#endif

// Data range and format definitions (should be defined here or included)
static KSDATAFORMAT_WAVEFORMATEXTENSIBLE Pcm48000_Stereo_16bit =
{
    {
        sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
    },
    {
        { WAVE_FORMAT_EXTENSIBLE, 2, 48000, 192000, 4, 16, sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) },
        16, KSAUDIO_SPEAKER_STEREO, STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
    }
};

static KSDATAFORMAT_WAVEFORMATEXTENSIBLE Pcm48000_16ch_16bit =
{
    {
        sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
    },
    {
        { WAVE_FORMAT_EXTENSIBLE, 16, 48000, 16 * 2 * 48000, 16 * 2, 16, sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) },
        16, 0, STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM) // Channel mask 0 for >2 channels often acceptable
    }
};

static KSDATARANGE_AUDIO PcmAudioDataRange =
{
    {
        sizeof(KSDATARANGE_AUDIO),
        0, // Flags
        LAMA_SHARED_RING_BUFFER_SIZE,       // SampleSize
        0,                                  // Reserved
        KSDATAFORMAT_TYPE_AUDIO,            // MajorFormat (GUID)
        KSDATAFORMAT_SUBTYPE_PCM,           // SubFormat (GUID)
        KSDATAFORMAT_SPECIFIER_WAVEFORMATEX // Specifier (GUID)
    },
    MAX_CHANNELS_PCM,                     // MaximumChannels
    MIN_BITS_PER_SAMPLE_PCM,              // MinimumBitsPerSample
    MAX_BITS_PER_SAMPLE_PCM,              // MaximumBitsPerSample
    MIN_SAMPLE_RATE_PCM,                  // MinimumSampleFrequency
    MAX_SAMPLE_RATE_PCM                   // MaximumSampleFrequency
};

static PKSDATARANGE PinDataRangesPcm[] =
{
    (PKSDATARANGE)&PcmAudioDataRange
};

// Pin Name string definitions (used by INF)
#define LAMA_LOOPBACK_RENDER_FRIENDLY_NAME    L"Lama Loopback Render"
#define LAMA_LOOPBACK_CAPTURE_FRIENDLY_NAME   L"Lama Loopback Capture"
#define LAMA_LOOPBACK_RENDER_BASENAME         L"LamaLoopbackRender"
#define LAMA_LOOPBACK_CAPTURE_BASENAME        L"LamaLoopbackCapture"

// Pin IDs for use in ENDPOINT_MINIPAIR (must match KSPIN_DESCRIPTOR_EX indices)
#define SystemRenderPin KSPIN_WAVE_HOST_IN
#define SystemCapturePin KSPIN_WAVE_CAPTURE_HOST_OUT
#define KSPIN_TOPO_BRIDGE_IN        0
#define KSPIN_TOPO_LOOPBACK_OUT     1
#define KSPIN_TOPO_LOOPBACK_IN      0
#define KSPIN_TOPO_BRIDGE_OUT       1
#define KSPIN_WAVE_HOST_IN          0
#define KSPIN_WAVE_BRIDGE_OUT       1
#define KSPIN_WAVE_CAPTURE_HOST_OUT 0
#define KSPIN_WAVE_BRIDGE_IN        1

#define INTERNAL_DRIVER_CHANNELS MAX_CHANNELS_PCM // Explicitly define for clarity if needed, here using MAX_CHANNELS_PCM
#define INTERNAL_DRIVER_BITS_PER_SAMPLE MAX_BITS_PER_SAMPLE_PCM // For internal consistency
