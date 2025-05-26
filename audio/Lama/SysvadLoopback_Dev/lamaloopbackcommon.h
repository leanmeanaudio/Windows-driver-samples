#pragma once

#include <ks.h>
#include <ksmedia.h>
#include <ntddk.h> 
#include <portcls.h> 

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
#define LAMA_SHARED_RING_BUFFER_SIZE (65536 * 2) 

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
#define MIN_CHANNELS_PCM    2       
#define MAX_CHANNELS_PCM    16      
#define DEFAULT_CHANNELS_PCM 2      
#define MIN_BITS_PER_SAMPLE_PCM   16      
#define MAX_BITS_PER_SAMPLE_PCM   16      
#define DEFAULT_BITS_PER_SAMPLE_PCM 16  

// extern KSDATAFORMAT_WAVEFORMATEXTENSIBLE Pcm48000_Stereo_16bit; 
// extern KSDATAFORMAT_WAVEFORMATEXTENSIBLE Pcm48000_16ch_16bit;   
// extern KSDATARANGE_AUDIO PcmAudioDataRange;                     
// extern PKSDATARANGE PinDataRangesPcm[];                         

// Shared Ring Buffer for Loopback (Per Instance)
typedef struct _LAMA_SHARED_LOOPBACK_BUFFER { 
    PBYTE           pBuffer; 
    ULONG           ulBufferSize; 
    volatile ULONG  ulWritePointer;
    volatile ULONG  ulReadPointer; 
    KSPIN_LOCK      SpinLock;
    // PDEVICE_OBJECT  pAssociatedDeviceObject; // Removed as buffer init is now global
    BOOLEAN         bInitialized;
} LAMA_SHARED_LOOPBACK_BUFFER, *PLAMA_SHARED_LOOPBACK_BUFFER;

extern LAMA_SHARED_LOOPBACK_BUFFER g_InstanceLoopbackBuffers[MAX_LAMA_INSTANCES];
NTSTATUS InitializeAllSharedLoopbackBuffers(VOID);
VOID FreeAllSharedLoopbackBuffers(VOID);

// Global Audio Format Variables (Existing definitions unchanged)
extern ULONG g_CurrentGlobalSampleRate;
extern ULONG g_CurrentGlobalChannels;
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
// Example: DEFINE_GUIDNAMED(PINNAME_LamaLoopbackWaveIn) ...
// These are referenced in lamaloopbackrender.h
// For simplicity, they are assumed to be defined elsewhere (e.g. a guids.h file included by common.h or directly here)
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
        0, //KSDATARANGE_ATTRIBUTES, // Specifies attributes for the data range.
        LAMA_SHARED_RING_BUFFER_SIZE, //LAMA_MAX_BUFFER_SIZE, // Maximum buffer size for this data range.
        0
    },
    KSDATAFORMAT_TYPE_AUDIO,
    KSDATAFORMAT_SUBTYPE_PCM,
    KSDATAFORMAT_SPECIFIER_WAVEFORMATEX,
    MIN_CHANNELS_PCM, MAX_CHANNELS_PCM, // Min/Max Channels
    MIN_BITS_PER_SAMPLE_PCM, MAX_BITS_PER_SAMPLE_PCM, // Min/Max Bits Per Sample
    MIN_SAMPLE_RATE_PCM, MAX_SAMPLE_RATE_PCM // Min/Max Sample Rate
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
#define SystemRenderPin KSPIN_WAVE_HOST_IN       // Typically 0
#define SystemCapturePin KSPIN_WAVE_CAPTURE_HOST_OUT // Typically 0
// Topology pin IDs (must match KSPIN_DESCRIPTOR_EX indices for each topology filter)
#define KSPIN_TOPO_BRIDGE_IN        0 
#define KSPIN_TOPO_LOOPBACK_OUT     1 
#define KSPIN_TOPO_LOOPBACK_IN      0 
#define KSPIN_TOPO_BRIDGE_OUT       1 
// Wave filter pin IDs (must match KSPIN_DESCRIPTOR_EX indices for each wave filter)
#define KSPIN_WAVE_HOST_IN          0
#define KSPIN_WAVE_BRIDGE_OUT       1
#define KSPIN_WAVE_CAPTURE_HOST_OUT 0
#define KSPIN_WAVE_BRIDGE_IN        1

// These were previously undefined and used as enum values in KSPIN_DESCRIPTOR_EX.
// Ensuring they are defined as constants for clarity, matching typical array indexing.
// Note: Their usage in KSPIN_DESCRIPTOR_EX `Id` field is as an index.
// The values assigned here must correspond to their position in their respective pin descriptor arrays.
// E.g., for LamaLoopbackRenderTopoPins:
//   LamaLoopbackRenderTopoPins[KSPIN_TOPO_BRIDGE_IN] is the first entry.
//   LamaLoopbackRenderTopoPins[KSPIN_TOPO_LOOPBACK_OUT] is the second entry.
// And so on for other pin arrays.
// This section just defines the constants; their usage in arrays determines their meaning.Okay, `lamaloopbackcommon.h` has been overwritten with the necessary changes, including:
-   `MAX_LAMA_INSTANCES` and `LAMA_LOOPBACK_DEVICE_MAX_INSTANCES` definitions.
-   `g_InstanceLoopbackBuffers` array declaration.
-   Updated function prototypes for `InitializeAllSharedLoopbackBuffers` and `FreeAllSharedLoopbackBuffers`.
-   Added definitions for Pin Name GUIDs and some DataFormat/DataRange structures that were previously only extern declared or missing. This is important for the descriptors in `lamaloopbackrender.h` to compile correctly.
-   Defined Pin ID constants (like `KSPIN_TOPO_BRIDGE_IN`, etc.) to ensure clarity, although their values are primarily derived from their order in descriptor arrays.

Now, I will proceed to **Step 2: Modify `sharedbuffer.cpp`**.

I will:
1.  Read `sharedbuffer.cpp`.
2.  Replace the single `g_SharedLoopbackBuffer` definition with the array `g_InstanceLoopbackBuffers[MAX_LAMA_INSTANCES]`.
3.  Implement `InitializeAllSharedLoopbackBuffers()` to loop through the array, allocate memory for each buffer, initialize spinlocks, and set other members.
4.  Implement `FreeAllSharedLoopbackBuffers()` to loop through the array and free allocated memory.
