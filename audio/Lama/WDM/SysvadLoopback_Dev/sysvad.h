/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    Sysvad.h

Abstract:

    Header file for common includes.

--*/

#ifndef _SYSVAD_H_
#define _SYSVAD_H_

// Include our C++ compatibility fix header before any other headers
#include "fix_ks_cpp.h"

// Keep existing compatibility headers
#include "driver_compat.h"

// Standard Windows headers
#include <portcls.h>
#include <stdunk.h>
#include <ksdebug.h>
#include <ksmedia.h>
#include <ntintsafe.h>
#include <wdf.h>
#include <wdfminiport.h>
#include <Ntstrsafe.h>
#include "Common\unknown.h"
#include "Common\kspindefs.h"

// Define SIZEOF_ARRAY macro to compute array size
#define SIZEOF_ARRAY(ar)          (sizeof(ar)/sizeof((ar)[0]))

// Function type definition for the miniport creation function
typedef NTSTATUS (*PFNCREATEMINIPORT)(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID Refclsid,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);

// Include common.h which has the PIN_DEVICE_FORMATS_AND_MODES definition already
#include "Common\common.h"

// Make SYSVAD_PIN_DEVICE_FORMATS_AND_MODES compatible with PIN_DEVICE_FORMATS_AND_MODES
#if !defined(SYSVAD_PIN_DEVICE_FORMATS_AND_MODES_DEFINED)
#define SYSVAD_PIN_DEVICE_FORMATS_AND_MODES_DEFINED

// Define an adapter structure that's compatible with PIN_DEVICE_FORMATS_AND_MODES
typedef struct _SYSVAD_PIN_DEVICE_FORMATS_AND_MODES {
    // The fields need to be compatible with PIN_DEVICE_FORMATS_AND_MODES
    PINTYPE                             PinType;  // First field matches PIN_DEVICE_FORMATS_AND_MODES
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE*  WaveFormats;
    ULONG                               WaveFormatsCount;
    MODE_AND_DEFAULT_FORMAT*            ModeAndDefaultFormat;
    ULONG                               ModeAndDefaultFormatCount;
} SYSVAD_PIN_DEVICE_FORMATS_AND_MODES, *PSYSVAD_PIN_DEVICE_FORMATS_AND_MODES;

// Forward declarations of pin device format arrays to be defined in individual driver files
extern SYSVAD_PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackRenderPinDeviceFormatsAndModes[];
extern SYSVAD_PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackCapturePinDeviceFormatsAndModes[];

// Function to help create pin device formats arrays more easily
inline void InitializeSysvadPinDeviceFormatsAndModes(
    SYSVAD_PIN_DEVICE_FORMATS_AND_MODES* deviceFormats,
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE* waveFormats,
    ULONG waveFormatsCount)
{
    if (deviceFormats) {
        deviceFormats->PinType = 1; // Use numeric value 1 (PINTYPE_DATA_IN) instead of enum
        deviceFormats->WaveFormats = waveFormats;
        deviceFormats->WaveFormatsCount = waveFormatsCount;
        deviceFormats->ModeAndDefaultFormat = NULL;
        deviceFormats->ModeAndDefaultFormatCount = 0;
    }
}

#endif // SYSVAD_PIN_DEVICE_FORMATS_AND_MODES_DEFINED

// Device Types - only define if not already defined in common.h
#if !defined(SYSVAD_EDEVICETYPE_DEFINED)
// For compatibility with existing code, use the same enum name but our own definition
typedef enum _SysvadDeviceType {
    eSpeakerDevice = 0,
    eSpeakerHpDevice,
    eHdmiRenderDevice,
    eMicInDevice,
    eMicArrayDevice1,
    eBthHfpMicDevice,
    eBthHfpSpeakerDevice,
    eUsbHsMicDevice,
    eUsbHsSpeakerDevice,
    eA2dpHpSpeakerDevice,
    eLoopbackMicDevice,
    eLoopbackSpeakerDevice,
    eTelephonyDevice,               // From original sysvad, ensure it's here if used
    eFmRxDevice,                    // From original sysvad
    eFmTxDevice,                    // From original sysvad
    eMicJackDevice,                 // From original sysvad
    eHeadphoneJackDevice,           // From original sysvad
    eLineInJackDevice,              // From original sysvad
    eSpdifInDevice,                 // From original sysvad
    eSpdifOutDevice,                // From original sysvad
    eSpeakerExtMicDevice,           // From original sysvad
    eHdmiRenderExtMicDevice,        // From original sysvad
    eHandsetDevice,                 // From original sysvad

    // Lama specific types
    eLamaLoopbackRenderDevice,
    eLamaLoopbackCaptureDevice,

    eDeviceTypeMax,                 // Max number of device types (should be last of actual types)
    eUnknownDevice = -1             // Placeholder for unknown device type (if used by sysvad)
} eDeviceType;
#endif // !defined(SYSVAD_EDEVICETYPE_DEFINED)

#define SYSVAD_DEVICE_TYPES (eDeviceTypeMax)

// Physical Connection Table
typedef enum {
    CONNECTIONTYPE_TOPOLOGY_OUTPUT = 0,
    CONNECTIONTYPE_WAVE_OUTPUT     = 1,
    CONNECTIONTYPE_TOPOLOGY_INPUT  = 2,
    CONNECTIONTYPE_WAVE_INPUT      = 3
} CONNECTIONTYPE;

// Using the PHYSICALCONNECTIONTABLE definition from common.h
// This is just for backward compatibility with code that might expect this layout
#ifndef SYSVAD_PHYSCONN_DEFINED
#define SYSVAD_PHYSCONN_DEFINED
typedef struct _SYSVAD_PHYSICAL_CONNECTION
{
    ULONG            ulTopology;
    ULONG            ulWave;
    CONNECTIONTYPE   eType;
} SYSVAD_PHYSICAL_CONNECTION, *PSYSVAD_PHYSICAL_CONNECTION;
#endif // SYSVAD_PHYSCONN_DEFINED

#endif // _SYSVAD_H_
