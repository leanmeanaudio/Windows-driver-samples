#pragma once

// Compatibility header for EndpointsCommon directory
// Provides necessary definitions for wave tables and other structures

// Include standard Windows headers in the correct order
#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

// Include Kernel Streaming headers
#include <ks.h>
#include <ksmedia.h>

// Required macros if not already defined
#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(ar) (sizeof(ar) / sizeof((ar)[0]))
#endif

// KSDATARANGE_ATTRIBUTES definition if not already defined
#ifndef KSDATARANGE_ATTRIBUTES
#define KSDATARANGE_ATTRIBUTES KSATTRIBUTE_REQUIRED
#endif

// Pin type definitions if not already defined
#ifndef SystemCapturePin
typedef enum {
    SystemRenderPin = 0,
    SystemCapturePin,
    RenderLoopbackPin,
    BridgePin
} PIN_TYPE;
#endif

// Required for wave tables
#ifndef STATIC_KSDATAFORMAT_TYPE_AUDIO
#define STATIC_KSDATAFORMAT_TYPE_AUDIO \
    0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
DEFINE_GUIDSTRUCT("73647561-0000-0010-8000-00aa00389b71", KSDATAFORMAT_TYPE_AUDIO);
#define KSDATAFORMAT_TYPE_AUDIO DEFINE_GUIDNAMED(KSDATAFORMAT_TYPE_AUDIO)
#endif

#ifndef STATIC_KSDATAFORMAT_SUBTYPE_PCM
#define STATIC_KSDATAFORMAT_SUBTYPE_PCM \
    0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
DEFINE_GUIDSTRUCT("00000001-0000-0010-8000-00aa00389b71", KSDATAFORMAT_SUBTYPE_PCM);
#define KSDATAFORMAT_SUBTYPE_PCM DEFINE_GUIDNAMED(KSDATAFORMAT_SUBTYPE_PCM)
#endif

#ifndef STATIC_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX
#define STATIC_KSDATAFORMAT_SPECIFIER_WAVEFORMATEX \
    0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a
DEFINE_GUIDSTRUCT("05589f81-c356-11ce-bf01-00aa0055595a", KSDATAFORMAT_SPECIFIER_WAVEFORMATEX);
#define KSDATAFORMAT_SPECIFIER_WAVEFORMATEX DEFINE_GUIDNAMED(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
#endif
