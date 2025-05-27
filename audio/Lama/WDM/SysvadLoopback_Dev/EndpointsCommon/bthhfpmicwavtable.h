// Include our audio driver compatibility header first
#include "audio_compat.h"

#include "waveformat_fix.h"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility

/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    bthhfpmicwavtable.h

Abstract:

    Declaration of wave miniport tables for the Bluetooth handls-free profile (external).

--*/

#ifndef _SYSVAD_BTHHFPMICWAVTABLE_H_
#define _SYSVAD_BTHHFPMICWAVTABLE_H_

// Include our compatibility header for EndpointsCommon
#include "endpoints_compat.h"

// Add necessary includes for audio driver development
#include <ntddk.h>     // Core NT definitions
#include <wdm.h>       // Windows Driver Model
#include <ksmedia.h>    // For audio specific definitions
#include <portcls.h>   // For PortCls definitions
#include "..\sysvad.h"  // This includes our custom kspindefs.h
#include <portcls.h>   // For audio port class definitions
#include <windef.h>    // For basic Windows types
#include "../sysvad.h"  // Include the project's main header file

// Define pin and node types only if not already defined
#ifndef KSPIN_WAVE_BRIDGE
#define KSPIN_WAVE_BRIDGE             2
#endif

#ifndef KSNODE_WAVE_ADC
#define KSNODE_WAVE_ADC               1
#endif
#define KSPIN_WAVEIN_HOST             0

// For simplicity, define common BT pin types (different from PINTYPE in common.h)
typedef enum {
    BtDataIn = 1,
    BtDataOut = 2
} BT_PINTYPE;

// Define maximum input streams
#define MAX_INPUT_STREAMS 1

// KSPROPSETID_AudioEffectsDiscovery and KSPROPERTY_AUDIOEFFECTSDISCOVERY_EFFECTSLIST 
// are now defined in kspindefs.h (included via sysvad.h)

// Create an empty structure for PinDataRangeAttributeList
static KSMULTIPLE_ITEM PinDataRangeAttributeList = { sizeof(KSMULTIPLE_ITEM), 0 };

//
// Function prototypes.
//
NTSTATUS PropertyHandler_BthHfpWaveFilter(_In_ PPCPROPERTY_REQUEST PropertyRequest);

//
// Bluetooth Headset Mic (external) range.
//
#define BTHHFPMIC_DEVICE_MAX_CHANNELS           1       // Max Channels.
#define BTHHFPMIC_MIN_BITS_PER_SAMPLE_PCM       16      // Min Bits Per Sample
#define BTHHFPMIC_MAX_BITS_PER_SAMPLE_PCM       16      // Max Bits Per Sample
#define BTHHFPMIC_MIN_SAMPLE_RATE               8000    // Min Sample Rate
#define BTHHFPMIC_MAX_SAMPLE_RATE               8000    // Max Sample Rate

// Additional defines needed for the driver
#define MAX_INPUT_STREAMS                        1       // Maximum number of input streams


//=============================================================================
static 
KSDATAFORMAT_WAVEFORMATEXTENSIBLE BthHfpMicPinSupportedDeviceFormats[] =
{
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
                1,
                8000,
                16000,
                2,
                16,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,
            KSAUDIO_SPEAKER_MONO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    }
};

//
// Supported modes (only on streaming pins) - NREC not supported.
//
static
MODE_AND_DEFAULT_FORMAT BthHfpMicPinSupportedDeviceModesNoNrec[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
        &BthHfpMicPinSupportedDeviceFormats[SIZEOF_ARRAY(BthHfpMicPinSupportedDeviceFormats)-1].DataFormat   
    }
};

//
// Supported modes (only on streaming pins) - NREC supported.
//
static
MODE_AND_DEFAULT_FORMAT BthHfpMicPinSupportedDeviceModesNrec[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
        &BthHfpMicPinSupportedDeviceFormats[SIZEOF_ARRAY(BthHfpMicPinSupportedDeviceFormats)-1].DataFormat   
    }
};

//
// The entries here must follow the same order as the filter's pin
// descriptor array.
//
// Hard-coded pin types to avoid conversion issues
#define PINTYPE_DATA_IN 1

static 
PIN_DEVICE_FORMATS_AND_MODES BthHfpMicPinDeviceFormatsAndModes[] = 
{
    {
        NoPin,    // PinType 
        NULL, // WaveFormats
        0,    // WaveFormatsCount
        NULL, // ModeAndDefaultFormat
        0     // ModeAndDefaultFormatCount
    },
    {
        SystemCapturePin,    // PinType
        BthHfpMicPinSupportedDeviceFormats, // WaveFormats
        SIZEOF_ARRAY(BthHfpMicPinSupportedDeviceFormats), // WaveFormatsCount
        BthHfpMicPinSupportedDeviceModesNrec, // ModeAndDefaultFormat
        SIZEOF_ARRAY(BthHfpMicPinSupportedDeviceModesNrec) // ModeAndDefaultFormatCount
    }
};

//=============================================================================
static
KSDATARANGE_AUDIO BthHfpMicPinDataRangesStream[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        BTHHFPMIC_DEVICE_MAX_CHANNELS,           
        BTHHFPMIC_MIN_BITS_PER_SAMPLE_PCM,    
        BTHHFPMIC_MAX_BITS_PER_SAMPLE_PCM,    
        BTHHFPMIC_MIN_SAMPLE_RATE,            
        BTHHFPMIC_MAX_SAMPLE_RATE             
    },
};

static
PKSDATARANGE BthHfpMicPinDataRangePointersStream[] =
{
    PKSDATARANGE(&BthHfpMicPinDataRangesStream[0])
    // Removed reference to undefined PinDataRangeAttributeList
};

//=============================================================================
static
KSDATARANGE BthHfpMicPinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    }
};

static
PKSDATARANGE BthHfpMicPinDataRangePointersBridge[] =
{
    &BthHfpMicPinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR BthHfpMicWaveMiniportPins[] =
{
    // Wave In Bridge Pin (Capture - From Topology) KSPIN_WAVE_BRIDGE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(BthHfpMicPinDataRangePointersBridge),
            BthHfpMicPinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
  
    // Wave In Streaming Pin (Capture) KSPIN_WAVEIN_HOST
    {
        MAX_INPUT_STREAMS,
        MAX_INPUT_STREAMS,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(BthHfpMicPinDataRangePointersStream),
            BthHfpMicPinDataRangePointersStream,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_RECORDING_CONTROL,  
            0
        }
    }
};

//=============================================================================
static
PCNODE_DESCRIPTOR BthHfpMicWaveMiniportNodes[] =
{
    // KSNODE_WAVE_ADC
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    }
};

//=============================================================================
static
PCCONNECTION_DESCRIPTOR BthHfpMicWaveMiniportConnections[] =
{
    { PCFILTER_NODE,        KSPIN_WAVE_BRIDGE,      KSNODE_WAVE_ADC,     1 },    
    { KSNODE_WAVE_ADC,      0,                      PCFILTER_NODE,       KSPIN_WAVEIN_HOST }
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesBthHfpMicWaveFilter[] =
{
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_BthHfpWaveFilter
    },
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_BthHfpWaveFilter
    },
    {
        &KSPROPSETID_Pin,  // Temporarily use KSPROPSETID_Pin instead of AudioEffectsDiscovery
        KSPROPERTY_PIN_NAME,  // Use a standard property instead
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_BthHfpWaveFilter
    }
};

NTSTATUS CMiniportWaveRT_EventHandler_PinCapsChange(
    _In_  PPCEVENT_REQUEST EventRequest
    );

static PCEVENT_ITEM BthHfpMicFormatChangePinEvent[] = {
    {
        &KSEVENTSETID_PinCapsChange,
        KSEVENT_PINCAPS_FORMATCHANGE,
        KSEVENT_TYPE_ENABLE | KSEVENT_TYPE_BASICSUPPORT,
        CMiniportWaveRT_EventHandler_PinCapsChange
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP_EVENT(AutomationBthHfpMicWaveFilter, PropertiesBthHfpMicWaveFilter, BthHfpMicFormatChangePinEvent);

//=============================================================================
static
PCFILTER_DESCRIPTOR BthHfpMicWaveMiniportFilterDescriptor =
{
    0,                                              // Version
    &AutomationBthHfpMicWaveFilter,                 // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(BthHfpMicWaveMiniportPins),        // PinCount
    BthHfpMicWaveMiniportPins,                      // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    SIZEOF_ARRAY(BthHfpMicWaveMiniportNodes),       // NodeCount
    BthHfpMicWaveMiniportNodes,                     // Nodes
    SIZEOF_ARRAY(BthHfpMicWaveMiniportConnections), // ConnectionCount
    BthHfpMicWaveMiniportConnections,               // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};

#endif // _SYSVAD_BTHHFPMICWAVTABLE_H_


