#pragma once

#include <wdm.h>
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>
#include <basetypes.h> // For BOOL

#include "lamaloopbackcommon.h" // For LAMA_POOL_TAG, MIN/MAX_SAMPLE_RATE_PCM, LAMA_MAX_BUFFER_SIZE etc.

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 16 // Default max channels if not in common.h
#endif

//=============================================================================
// CMiniportWaveRTLamaLoopbackStream
//=============================================================================
class CMiniportWaveRTLamaLoopbackStream :
    public IMiniportWaveRTStream,
    public CUnknown
{
public:
    // Constructor and Destructor
    CMiniportWaveRTLamaLoopbackStream(PUNKNOWN OuterUnknown);
    ~CMiniportWaveRTLamaLoopbackStream();

    // IUnknown methods
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTLamaLoopbackStream);

    // IMiniportWaveRTStream methods
    STDMETHODIMP_(NTSTATUS) SetFormat(
        _In_  PKSDATAFORMAT DataFormat
    );

    STDMETHODIMP_(NTSTATUS) SetState(
        _In_  KSSTATE KsState
    );

    STDMETHODIMP_(NTSTATUS) GetPosition(
        _Out_ PKSAUDIO_POSITION Position
    );

    STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(
        _In_  ULONG RequestedSize,
        _Out_ PMDL  *AudioBufferMdl, 
        _Out_ ULONG *ActualSize,
        _Out_ ULONG *OffsetFromFirstPage,
        _Out_ MEMORY_CACHING_TYPE *CacheType
    );

    STDMETHODIMP_(VOID) FreeAudioBuffer(
        _In_opt_ PMDL AudioBufferMdl, 
        _In_ ULONG BufferSize        
    );

    STDMETHODIMP_(NTSTATUS) GetClock(
        _Out_ HANDLE *ClockHandle
    );

    STDMETHODIMP_(NTSTATUS) GetDeviceProperty(
        _In_      REFGUID                       rguidPropSet,
        _In_      ULONG                         ulPropId,
        _In_      ULONG                         ulPropValueSize,
        _Out_writes_bytes_opt_(ulPropValueSize) PVOID                         pvPropValue,
        _Out_     PULONG                        pulReturnBytes
    );

    STDMETHODIMP_(NTSTATUS) SetDeviceProperty(
        _In_ REFGUID  rguidPropSet,
        _In_ ULONG    ulPropId,
        _In_ ULONG    ulPropValueSize,
        _In_reads_bytes_(ulPropValueSize) PVOID    pvPropValue
    );

    STDMETHODIMP_(NTSTATUS) GetPositionRegister(
        _Out_ KSRTAUDIO_HWREGISTER* Register
    );

    STDMETHODIMP_(NTSTATUS) GetHwLatency(
        _Out_ KSRTAUDIO_HWLATENCY* HwLatency
    );

    STDMETHODIMP_(NTSTATUS) GetHardwareDescription(
        _Out_    PENDPOINT_MINIPAIR    Minipair,
        _Outptr_result_maybenull_ PVOID*               DeviceContext
    );

    STDMETHODIMP_(NTSTATUS) GetSupportedDeviceFormats(
        _In_  ULONG                       ulFormatCount,
        _Out_writes_bytes_opt_(ulFormatCount * sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)) PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFormatArray
    );

    STDMETHODIMP_(NTSTATUS) GetSupportedDeviceProperties(
        _In_  ULONG                       ulPropertyCount,
        _Out_writes_bytes_opt_(ulPropertyCount * sizeof(KSPROPERTY_ITEM)) PKSPROPERTY_ITEM pPropertyArray
    );

    STDMETHODIMP_(NTSTATUS) GetSupportedDeviceEvents(
        _In_  ULONG                       ulEventCount,
        _Out_writes_bytes_opt_(ulEventCount * sizeof(KSEVENT_ITEM)) PKSEVENT_ITEM pEventArray
    );

    STDMETHODIMP_(NTSTATUS) GetVolume(
        _In_  ULONG                       ulChannel,
        _Out_ PULONG                      pulValue
    );

    STDMETHODIMP_(NTSTATUS) SetVolume(
        _In_ ULONG                       ulChannel,
        _In_ ULONG                       ulValue
    );

    STDMETHODIMP_(NTSTATUS) GetMute(
        _Out_ PBOOL                       pbValue
    );

    STDMETHODIMP_(NTSTATUS) SetMute(
        _In_ BOOL                        bValue
    );

    // Initialization method
    NTSTATUS Init(
        _In_ PPORTWAVERTSTREAM PortStream,
        _In_ PKSDATAFORMAT DataFormat,
        _In_ BOOLEAN Capture,
        _In_ ULONG InstanceIndex // New parameter
    );

    // Internal helper methods for data transfer with shared buffer
    // These would be called, for example, from a DPC routine triggered by WaveRT notifications
    VOID ProcessRenderDataFromWaveRtBuffer(ULONG ulCurrentWaveRtBufferOffset, ULONG ulByteCount);
    VOID FetchCaptureDataToWaveRtBuffer(ULONG ulCurrentWaveRtBufferOffset, ULONG ulByteCount);


private:
    PPORTWAVERTSTREAM       m_pPortStream;      
    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE m_pDataFormat; 
    KSSTATE                 m_KsState;          
    BOOLEAN                 m_bCapture;         
    ULONG                   m_instanceIndex;        // Index for the per-instance shared buffer

    PMDL                    m_pAudioBufferMdl;      
    PVOID                   m_pAudioBuffer;         // KVA of m_pAudioBufferMdl
    ULONG                   m_ulCurrentBufferSize;  // Actual size of m_pAudioBuffer
    ULONG                   m_ulMaxBufferSize;      
    
    ULONG                   m_ulChannelCount;
    ULONG                   m_ulSampleRate;
    ULONG                   m_ulBitsPerSample;

    KTIMER                  m_NotificationTimer;    
    HANDLE                  m_NotificationEvent;    // Event from PortCls for WaveRT notifications
    KDPC                    m_NotificationDpc;      // DPC for handling notifications (example)
    ULONG                   m_ulDmaMovementRate;    // How often DMA moves, for GetPosition simulation
    ULONGLONG               m_ullPlayPosition;      // Stream's own position counter
    ULONGLONG               m_ullWritePosition;     // Stream's own position counter


public:
    // NonDelegatingQueryInterface
    STDMETHODIMP
    NonDelegatingQueryInterface
    (
        _In_         REFIID  Interface,
        _COM_Outptr_ PVOID   *Object
    );

    // DPC routine (example, would need to be initialized and queued)
    static VOID NotificationDpcRoutine(
        _In_ PKDPC Dpc,
        _In_opt_ PVOID DeferredContext,
        _In_opt_ PVOID SystemArgument1,
        _In_opt_ PVOID SystemArgument2
    );

    // New methods for direct IRP data handling (WriteFile/ReadFile)
    NTSTATUS HandleWriteIRPData(
        _In_reads_bytes_(ulByteCount) PVOID pData,
        _In_ ULONG ulByteCount
    );

    NTSTATUS HandleReadIRPData(
        _Out_writes_bytes_to_(ulReqSize, *pulBytesCopied) PVOID pData,
        _In_ ULONG ulReqSize,
        _Out_ PULONG pulBytesCopied
    );
};

typedef CMiniportWaveRTLamaLoopbackStream *PCMiniportWaveRTLamaLoopbackStream;
