#pragma once

#include <ks.h>
#include <ksmedia.h>
#include <ntddk.h> 
#include <portcls.h> 

// Pool Tag
#define LAMA_POOL_TAG 'BLSL' 

// Buffer size constants
#define LAMA_MAX_STREAM_BUFFER_SIZE (1024 * 1024)      
#define LAMA_DEFAULT_PACKET_SIZE_MS 10          
#define LAMA_SHARED_RING_BUFFER_SIZE (65536 * 2) 

// Pin properties
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

// Node properties
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

// Data Formats & Ranges
#define MIN_SAMPLE_RATE_PCM       8000    
#define MAX_SAMPLE_RATE_PCM       192000  
#define DEFAULT_SAMPLE_RATE_PCM   48000   
#define MIN_CHANNELS_PCM    2       
#define MAX_CHANNELS_PCM    16      
#define DEFAULT_CHANNELS_PCM 2      
#define MIN_BITS_PER_SAMPLE_PCM   16      
#define MAX_BITS_PER_SAMPLE_PCM   16      
#define DEFAULT_BITS_PER_SAMPLE_PCM 16  

// extern KSDATAFORMAT_WAVEFORMATEXTENSIBLE Pcm48000_Stereo_16bit; // Defined in cpp if needed globally
// extern KSDATAFORMAT_WAVEFORMATEXTENSIBLE Pcm48000_16ch_16bit;   // Defined in cpp if needed globally
// extern KSDATARANGE_AUDIO PcmAudioDataRange;                     // Defined in cpp if needed globally
// extern PKSDATARANGE PinDataRangesPcm[];                         // Defined in cpp if needed globally

// Shared Ring Buffer for Loopback
typedef struct _LAMA_SHARED_LOOPBACK_BUFFER { 
    PBYTE           pBuffer; ULONG ulBufferSize; volatile ULONG  ulWritePointer;
    volatile ULONG  ulReadPointer; KSPIN_LOCK SpinLock;
    PDEVICE_OBJECT  pAssociatedDeviceObject; BOOLEAN bInitialized;
} LAMA_SHARED_LOOPBACK_BUFFER, *PLAMA_SHARED_LOOPBACK_BUFFER;

extern LAMA_SHARED_LOOPBACK_BUFFER g_SharedLoopbackBuffer;
NTSTATUS InitializeSharedLoopbackBuffer(PDEVICE_OBJECT DeviceObject);
VOID FreeSharedLoopbackBuffer(VOID);

// Global Audio Format Variables
extern ULONG g_CurrentGlobalSampleRate;
extern ULONG g_CurrentGlobalChannels;
extern ULONG g_CurrentGlobalBitsPerSample;

// Lama Loopback Sample Rate Control Property Set
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

// Debugging DPF levels and macros
#ifndef DPF_LEVEL_ERROR
#define DPF_LEVEL_ERROR     DPFLTR_ERROR_LEVEL
#endif
// ... (other DPF defines as before)
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
