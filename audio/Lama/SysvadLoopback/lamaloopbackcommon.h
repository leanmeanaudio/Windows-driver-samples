#pragma once

#include <ks.h>
#include <ksmedia.h>

//=============================================================================
// Defines
//=============================================================================

//
// Pin properties.
//
#define STATIC_PIN_IDS_WAVE_RENDER\
    STATIC_KSPIN_WAVE_RENDER_SINK_IN, \
    STATIC_KSPIN_WAVE_RENDER_SOURCE_OUT

#define STATIC_PIN_IDS_WAVE_CAPTURE\
    STATIC_KSPIN_WAVE_CAPTURE_SOURCE_IN, \
    STATIC_KSPIN_WAVE_CAPTURE_SINK_OUT

#define STATIC_PIN_IDS_TOPO_RENDER\
    STATIC_KSPIN_TOPO_RENDER_BRIDGE_IN, \
    STATIC_KSPIN_TOPO_RENDER_LOOPBACK_OUT

#define STATIC_PIN_IDS_TOPO_CAPTURE\
    STATIC_KSPIN_TOPO_CAPTURE_LOOPBACK_IN, \
    STATIC_KSPIN_TOPO_CAPTURE_BRIDGE_OUT

//
// Node properties for render
//
#define STATIC_KSNODE_TOPO_RENDER_VOLUME    STATIC_KSNODEID_SYSAUDIO_VOLUME
#define STATIC_KSNODE_TOPO_RENDER_MUTE      STATIC_KSNODEID_SYSAUDIO_MUTE

//
// Node properties for capture
//
#define STATIC_KSNODE_TOPO_CAPTURE_VOLUME   STATIC_KSNODEID_SYSAUDIO_VOLUME
#define STATIC_KSNODE_TOPO_CAPTURE_MUTE     STATIC_KSNODEID_SYSAUDIO_MUTE


//=============================================================================
// Data Formats
//=============================================================================

//
// PCM 48kHz, 16 channels, 16 bits per sample
//
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
        {
            WAVE_FORMAT_EXTENSIBLE,
            16,     // nChannels
            48000,  // nSamplesPerSec
            1536000,// nAvgBytesPerSec (48000 * 16 * 2) - Corrected
            32,     // nBlockAlign (16 * 2) - Corrected
            16,     // wBitsPerSample
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
        },
        16,     // Samples.wValidBitsPerSample
        0,      // dwChannelMask (0 for direct out or high channel counts)
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
    }
};

//
// Standard formats for the pins.
//
static KSDATARANGE_AUDIO Pcm48000_16ch_16bit_Range =
{
    {
        sizeof(KSDATARANGE_AUDIO),
        KSDATARANGE_ATTRIBUTES,         // An attributes flag defined in ksmedia.h
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
    },
    16,      // Maximum channels
    8,       // Minimum bits per sample
    16,      // Maximum bits per sample
    1000,    // Minimum rate
    384000   // Maximum rate
};

// Convenience array for data ranges.
static PKSDATARANGE PinDataRangesPcm[] =
{
    (PKSDATARANGE)&Pcm48000_16ch_16bit_Range
};

//=============================================================================
// Pin, Node, Connection and Filter Descriptors
//=============================================================================

//
// Pin Names
//
#define KSPIN_TOPO_BRIDGE_IN        0
#define KSPIN_TOPO_LOOPBACK_OUT     1

#define KSPIN_WAVE_HOST_IN          0
#define KSPIN_WAVE_BRIDGE_OUT       1

#define KSPIN_TOPO_LOOPBACK_IN      0
#define KSPIN_TOPO_BRIDGE_OUT       1

#define KSPIN_WAVE_BRIDGE_IN        0
#define KSPIN_WAVE_HOST_OUT         1

//
// Node IDs (NONE for direct passthrough)
//
#define KS auriculNODE_NONE (-1)

//=============================================================================
// Helper Defines for Descriptors (from Sysvad EndpointsCommon)
//=============================================================================
#define MIN_CHANNELS        2       // Min Channels.
#define MAX_CHANNELS_PCM    16      // Max Channels.
#define MIN_BITS_PER_SAMPLE_PCM   16      // Min Bits Per Sample
#define MAX_BITS_PER_SAMPLE_PCM   16      // Max Bits Per Sample
#define MIN_SAMPLE_RATE_PCM       48000   // Min Sample Rate
#define MAX_SAMPLE_RATE_PCM       48000   // Max Sample Rate

#define SPEAKER_RAW_DATA_RANGE_COUNT    1
#define SPEAKER_DEFAULT_DATA_RANGE_COUNT 1

#define MIC_RAW_DATA_RANGE_COUNT    1
#define MIC_DEFAULT_DATA_RANGE_COUNT 1

// Specific GUIDs for categories, these can be defined as needed or use existing ones.
// For simplicity, using KSCATEGORY_AUDIO for all pins.
// DEFINE_GUIDSTRUCT("...", KSCATEGORY_LAMA_LOOPBACK_RENDER_TOPOLOGY);
// DEFINE_GUIDSTRUCT("...", KSCATEGORY_LAMA_LOOPBACK_RENDER_WAVE);
// DEFINE_GUIDSTRUCT("...", KSCATEGORY_LAMA_LOOPBACK_CAPTURE_TOPOLOGY);
// DEFINE_GUIDSTRUCT("...", KSCATEGORY_LAMA_LOOPBACK_CAPTURE_WAVE);

// Define a unique name for our loopback device interface
DEFINE_GUIDNAMED(KSCATEGORY_LAMA_LOOPBACK,
0xe5a8c3e6, 0x9c7a, 0x4beb, 0x91, 0x58, 0x13, 0xe, 0x70, 0x80, 0x5f, 0x47); // Example GUID, generate a new one

// Define pin category guids if specific ones are needed, otherwise use KSCATEGORY_AUDIO
DEFINE_GUIDNAMED(PINNAME_LamaLoopbackWaveIn,
0xf222d386, 0xbf7a, 0x4842, 0x92, 0x5d, 0x86, 0x44, 0x21, 0x27, 0x80, 0x70); // Example GUID
DEFINE_GUIDNAMED(PINNAME_LamaLoopbackWaveOut,
0xa37ca70f, 0x498c, 0x4490, 0x8a, 0x1d, 0x58, 0x46, 0x47, 0x47, 0x81, 0xf9); // Example GUID

// Bridge pin GUIDs (from Sysvad tablet)
DEFINE_GUIDNAMED(LAMA_LOOPBACK_BRIDGE_PIN_IN,  // Render Wave Out / Capture Topo In
0x10a7a1e0, 0x710a, 0x4484, 0x9d, 0x4e, 0xd5, 0x47, 0x29, 0xf6, 0xb7, 0x54); // Example GUID
DEFINE_GUIDNAMED(LAMA_LOOPBACK_BRIDGE_PIN_OUT, // Render Topo Out / Capture Wave In
0x87a7a2e0, 0x710a, 0x4484, 0x9d, 0x4e, 0xd5, 0x47, 0x29, 0xf6, 0xb7, 0x55); // Example GUID

#define KSPIN_LAMA_LOOPBACK_WAVE_BRIDGE KSPIN_WAVE_BRIDGE_OUT // Render wave output pin for loopback
#define KSPIN_LAMA_LOOPBACK_TOPO_BRIDGE KSPIN_TOPO_LOOPBACK_IN // Capture topo input pin for loopback

//=============================================================================
// Shared Ring Buffer for Loopback
//=============================================================================
#define LAMA_LOOPBACK_BUFFER_SIZE (65536) // 64KB buffer

typedef struct _LAMA_SHARED_LOOPBACK_BUFFER
{
    BYTE*           pBuffer;                    // Pointer to the allocated buffer
    ULONG           ulBufferSize;               // Size of the buffer
    volatile ULONG  ulWritePointer;             // Write pointer (updated by render stream)
    volatile ULONG  ulReadPointer;              // Read pointer (updated by capture stream)
    KSPIN_LOCK      SpinLock;                   // For synchronizing access to pointers and buffer
    PDEVICE_OBJECT  pAssociatedDeviceObject;    // Associated FDO, for potential debugging or context
} LAMA_SHARED_LOOPBACK_BUFFER, *PLAMA_SHARED_LOOPBACK_BUFFER;

//=============================================================================
// Lama Loopback Sample Rate Control Property Set
//=============================================================================
// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} - Generate a new GUID for this
DEFINE_GUIDSTRUCT("7C4E6248-4C7D-4FE4-8E4F-4E62484C7D00", KSPROPSETID_LamaLoopback); // Example GUID
#define KSPROPSETID_LamaLoopback DEFINE_GUIDNAMED(KSPROPSETID_LamaLoopback)

typedef enum {
    KSPROPERTY_LAMA_SAMPLE_RATE = 0
} KSPROPERTY_LAMA;
