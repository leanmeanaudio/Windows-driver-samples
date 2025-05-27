//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#ifndef _SYSVAD_LAMALOOPBACKWAVTABLE_H_
#define _SYSVAD_LAMALOOPBACKWAVTABLE_H_

#include <portcls.h>
#include <ksmedia.h>
#include "sysvad.h"          // For MINIFILTER_DESCRIPTOR, CreateMiniportWaveRTSYSVAD, etc.
#include "endpointscommon.h" // For MiniportWaveSimpleAutomation, PIN_DEVICE_FORMATS_AND_MODES, etc.
#include "pcstrmif.h"        // For MiniportWaveRTCSharpStreamCallbacks

//=============================================================================
// Defines
//=============================================================================
#define LAMA_LOOPBACK_MAX_CHANNELS        16
#define LAMA_LOOPBACK_BITS_PER_SAMPLE     16

//=============================================================================
// Data Ranges for 16-channel, 16-bit PCM
//=============================================================================
static const KSDATARANGE_AUDIO LamaLoopback_DataRanges[] =
{
    { // 44.1 kHz
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            STATICGUID(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUID(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUID(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        LAMA_LOOPBACK_MAX_CHANNELS,
        LAMA_LOOPBACK_BITS_PER_SAMPLE,
        LAMA_LOOPBACK_BITS_PER_SAMPLE,
        44100, // MinSampleFrequency
        44100  // MaxSampleFrequency
    },
    { // 48 kHz
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            STATICGUID(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUID(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUID(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        LAMA_LOOPBACK_MAX_CHANNELS,
        LAMA_LOOPBACK_BITS_PER_SAMPLE,
        LAMA_LOOPBACK_BITS_PER_SAMPLE,
        48000, // MinSampleFrequency
        48000  // MaxSampleFrequency
    },
    { // 96 kHz
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            STATICGUID(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUID(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUID(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        LAMA_LOOPBACK_MAX_CHANNELS,
        LAMA_LOOPBACK_BITS_PER_SAMPLE,
        LAMA_LOOPBACK_BITS_PER_SAMPLE,
        96000, // MinSampleFrequency
        96000  // MaxSampleFrequency
    }
};

static PKSDATARANGE LamaLoopback_DataRangePointers[] =
{
    (PKSDATARANGE)&LamaLoopback_DataRanges[0],
    (PKSDATARANGE)&LamaLoopback_DataRanges[1],
    (PKSDATARANGE)&LamaLoopback_DataRanges[2]
};

//=============================================================================
// Audio Signal Processing Modes
//=============================================================================
static AUDIO_SIGNALPROCESSINGMODE LamaLoopback_AudioSignalProcessingModes[] =
{
    AUDIO_SIGNALPROCESSINGMODE_DEFAULT
};

//=============================================================================
// Pin Formats and Modes
// This structure is used by lamaloopbackminipairs.h
//=============================================================================
const PIN_DEVICE_FORMATS_AND_MODES LamaLoopback_PinDeviceFormatsAndModes[] =
{
    {
        &KSNODETYPE_AUDIO_ENGINE, // PinCategory
        NULL,                     // PinName (bridge pins don't typically have names)
        LamaLoopback_DataRangePointers,
        SIZEOF_ARRAY(LamaLoopback_DataRangePointers),
        LamaLoopback_AudioSignalProcessingModes,
        SIZEOF_ARRAY(LamaLoopback_AudioSignalProcessingModes)
    }
};

//=============================================================================
// Wave Miniport Pin Descriptors
//=============================================================================
static const KSPIN_DESCRIPTOR LamaLoopbackRender_WavePins[] =
{
    { // KSPIN_WAVE_BRIDGE (Sink)
        0, // InterfacesCount
        NULL, // Interfaces
        0, // MediumsCount
        NULL, // Mediums
        SIZEOF_ARRAY(LamaLoopback_DataRangePointers), // DataRangesCount
        LamaLoopback_DataRangePointers, // DataRanges
        KSPIN_DATAFLOW_IN, // DataFlow
        KSPIN_COMMUNICATION_BRIDGE, // Communication
        NULL, // Category (Audio Engine for filter, NULL for bridge pin)
        NULL, // Name (NULL for bridge pin)
        0     // ConstrainedDataRangesCount
    }
};

static const KSPIN_DESCRIPTOR LamaLoopbackCapture_WavePins[] =
{
    { // KSPIN_WAVE_BRIDGE (Source)
        0, // InterfacesCount
        NULL, // Interfaces
        0, // MediumsCount
        NULL, // Mediums
        SIZEOF_ARRAY(LamaLoopback_DataRangePointers), // DataRangesCount
        LamaLoopback_DataRangePointers, // DataRanges
        KSPIN_DATAFLOW_OUT, // DataFlow
        KSPIN_COMMUNICATION_BRIDGE, // Communication
        NULL, // Category (Audio Engine for filter, NULL for bridge pin)
        NULL, // Name (NULL for bridge pin)
        0     // ConstrainedDataRangesCount
    }
};

//=============================================================================
// Wave Miniport Filter Descriptors
// These are used by lamaloopbackminipairs.h
//=============================================================================
const MINIFILTER_DESCRIPTOR LamaLoopbackRenderWaveMiniportFilterDescriptor =
{
    MINIFILTER_DESCRIPTOR_FLAGS_VERSION, // FlagsVersion
    &MiniportWaveSimpleAutomation, // AutomationTable
    sizeof(KSPIN_DESCRIPTOR), // PinSize
    SIZEOF_ARRAY(LamaLoopbackRender_WavePins), // PinCount
    (PKSPIN_DESCRIPTOR)LamaLoopbackRender_WavePins, // Pins
    0, // NodeSize
    0, // NodeCount
    NULL, // Nodes
    0, // ConnectionSize
    0, // ConnectionCount
    NULL, // Connections
    0, // CategorySize
    0, // CategoryCount
    NULL, // Categories
    CreateMiniportWaveRTSYSVAD, // MiniportCreate
    DEFINE_KSFILTER_DESCRIPTOR(NULL), // Name
    LamaLoopback_PinDeviceFormatsAndModes,
    SIZEOF_ARRAY(LamaLoopback_PinDeviceFormatsAndModes),
    LAMA_LOOPBACK_MAX_CHANNELS, // DeviceMaxChannelsOverride
    &MiniportWaveRTCSharpStreamCallbacks // Callbacks
};

const MINIFILTER_DESCRIPTOR LamaLoopbackCaptureWaveMiniportFilterDescriptor =
{
    MINIFILTER_DESCRIPTOR_FLAGS_VERSION, // FlagsVersion
    &MiniportWaveSimpleAutomation, // AutomationTable
    sizeof(KSPIN_DESCRIPTOR), // PinSize
    SIZEOF_ARRAY(LamaLoopbackCapture_WavePins), // PinCount
    (PKSPIN_DESCRIPTOR)LamaLoopbackCapture_WavePins, // Pins
    0, // NodeSize
    0, // NodeCount
    NULL, // Nodes
    0, // ConnectionSize
    0, // ConnectionCount
    NULL, // Connections
    0, // CategorySize
    0, // CategoryCount
    NULL, // Categories
    CreateMiniportWaveRTSYSVAD, // MiniportCreate
    DEFINE_KSFILTER_DESCRIPTOR(NULL), // Name
    LamaLoopback_PinDeviceFormatsAndModes, // Same formats and modes for capture
    SIZEOF_ARRAY(LamaLoopback_PinDeviceFormatsAndModes),
    LAMA_LOOPBACK_MAX_CHANNELS, // DeviceMaxChannelsOverride
    &MiniportWaveRTCSharpStreamCallbacks // Callbacks
};

// As requested by the task, provide extern const declarations for items needed by lamaloopbackminipairs.h.
// Note: Since these are defined as 'const' (not 'static const') in this header, 
// including this header is sufficient for other files to link against them.
// Explicit 'extern const' here is redundant but harmless if it's a specific project convention.
// For typical header-only definitions, these 'extern' lines would not be present.
extern const MINIFILTER_DESCRIPTOR LamaLoopbackRenderWaveMiniportFilterDescriptor;
extern const MINIFILTER_DESCRIPTOR LamaLoopbackCaptureWaveMiniportFilterDescriptor;
extern const PIN_DEVICE_FORMATS_AND_MODES LamaLoopback_PinDeviceFormatsAndModes[];

#endif // _SYSVAD_LAMALOOPBACKWAVTABLE_H_
