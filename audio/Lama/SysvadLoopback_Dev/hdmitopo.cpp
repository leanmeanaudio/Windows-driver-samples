#include "hdmitopo.h"
#include "lamaloopbackcommon.h" // For LAMA_POOL_TAG and format constants
#include <ntstatus.h>           // For STATUS_SUCCESS, etc.
#include <ksdebug.h>            // For DPF_ENTER, DPF_LEAVE, etc. (if used by common.h)

// Ensure DPF macros are defined if not pulled from ksdebug.h via common.h
#ifndef DPF_ENTER
#define DPF_ENTER(x) DPF(DPF_LEVEL_TRACE, ("Entered " x))
#endif
#ifndef DPF_LEAVE
#define DPF_LEAVE(x) DPF(DPF_LEVEL_TRACE, ("Exiting " x))
#endif
#ifndef DPF
  #if DBG
    #define DPF(lvl, _x_) DbgPrint _x_
  #else
    #define DPF(lvl, _x_)
  #endif
#endif


#pragma code_seg("PAGE")
//=============================================================================
// CMiniportWaveRTLamaLoopbackStream Implementation
//=============================================================================

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::CMiniportWaveRTLamaLoopbackStream
//-----------------------------------------------------------------------------
CMiniportWaveRTLamaLoopbackStream::CMiniportWaveRTLamaLoopbackStream
(
    _In_opt_ PUNKNOWN OuterUnknown
)
: CUnknown(OuterUnknown),
  m_pPortStream(NULL),
  m_pDataFormat(NULL),
  m_KsState(KSSTATE_STOP),
  m_bCapture(FALSE),
  m_instanceIndex(0), // Initialize instance index
  m_pAudioBufferMdl(NULL),
  m_pAudioBuffer(NULL),      
  m_ulCurrentBufferSize(0),
  m_ulMaxBufferSize(LAMA_MAX_STREAM_BUFFER_SIZE), 
  m_ulChannelCount(0),
  m_ulSampleRate(0),
  m_ulBitsPerSample(0),
  m_NotificationEvent(NULL),
  m_ulDmaMovementRate(0), 
  m_ullPlayPosition(0),
  m_ullWritePosition(0)
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::CMiniportWaveRTLamaLoopbackStream"));
    RtlZeroMemory(&m_NotificationTimer, sizeof(m_NotificationTimer)); 
    RtlZeroMemory(&m_NotificationDpc, sizeof(m_NotificationDpc)); 
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::~CMiniportWaveRTLamaLoopbackStream
//-----------------------------------------------------------------------------
CMiniportWaveRTLamaLoopbackStream::~CMiniportWaveRTLamaLoopbackStream()
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::~CMiniportWaveRTLamaLoopbackStream"));

    if (m_pDataFormat)
    {
        ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG);
        m_pDataFormat = NULL;
    }

    if (m_pAudioBufferMdl || m_pAudioBuffer) 
    {
        FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); 
    }
    
    if (m_NotificationEvent)
    {
        m_NotificationEvent = NULL; 
    }

    if (m_pPortStream)
    {
        m_pPortStream->Release();
        m_pPortStream = NULL;
    }
    
    DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::~CMiniportWaveRTLamaLoopbackStream"));
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::NonDelegatingQueryInterface
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::NonDelegatingQueryInterface
(
    _In_ REFIID Interface,
    _COM_Outptr_ PVOID *Object
)
{
    PAGED_CODE();
    ASSERT(Object);

    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::NonDelegatingQueryInterface"));

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTStream))
    {
        *Object = PVOID(PMINIPORTWAVERTSTREAM(this));
    }
    else
    {
        *Object = NULL;
        DPF(DPF_LEVEL_TERSE, ("NonDelegatingQueryInterface: Interface not supported"));
        return STATUS_NOT_SUPPORTED;
    }

    ((PUNKNOWN)*Object)->AddRef();
    DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::NonDelegatingQueryInterface, status=STATUS_SUCCESS"));
    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::Init
//-----------------------------------------------------------------------------
NTSTATUS
CMiniportWaveRTLamaLoopbackStream::Init
(
    _In_ PPORTWAVERTSTREAM PortStream,
    _In_ PKSDATAFORMAT DataFormat,
    _In_ BOOLEAN Capture,
    _In_ ULONG InstanceIndex // New parameter
)
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::Init - Instance: %u", InstanceIndex));

    ASSERT(PortStream);
    ASSERT(DataFormat);
    ASSERT(InstanceIndex < MAX_LAMA_INSTANCES); // Basic validation

    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_pPortStream = PortStream;
    m_pPortStream->AddRef();
    m_bCapture = Capture;
    m_instanceIndex = InstanceIndex; // Store the instance index

    if (!IsEqualGUIDAligned(DataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(DataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(DataFormat->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DPF(DPF_LEVEL_ERROR, ("Init: Unsupported DataFormat Major/Sub/Specifier."));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }

    PKSDATAFORMAT_WAVEFORMATEX pKsDataFormatWfx = reinterpret_cast<PKSDATAFORMAT_WAVEFORMATEX>(DataFormat);
    WAVEFORMATEX* wfex = &pKsDataFormatWfx->WaveFormatEx;

    m_ulSampleRate    = wfex->nSamplesPerSec;
    m_ulChannelCount  = wfex->nChannels;
    m_ulBitsPerSample = wfex->wBitsPerSample;

    if (m_ulSampleRate < MIN_SAMPLE_RATE_PCM || m_ulSampleRate > MAX_SAMPLE_RATE_PCM ||
        (wfex->wFormatTag != WAVE_FORMAT_PCM && wfex->wFormatTag != WAVE_FORMAT_EXTENSIBLE) )
    {
        DPF(DPF_LEVEL_ERROR, ("Init: Unsupported Sample Rate (%u) or Format Tag (0x%X).", m_ulSampleRate, wfex->wFormatTag));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    if (m_ulBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || m_ulBitsPerSample > MAX_BITS_PER_SAMPLE_PCM)
    {
        DPF(DPF_LEVEL_ERROR, ("Init: Unsupported Bits Per Sample: %u", m_ulBitsPerSample));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    if (m_ulChannelCount < MIN_CHANNELS_PCM || m_ulChannelCount > MAX_CHANNELS_PCM)
    {
        DPF(DPF_LEVEL_ERROR, ("Init: Unsupported Channel Count: %u", m_ulChannelCount));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    
    if (m_pDataFormat)
    {
        ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG);
    }
    m_pDataFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE) ExAllocatePoolWithTag(
                        NonPagedPoolNx, DataFormat->FormatSize, LAMA_POOL_TAG);
    if (!m_pDataFormat)
    {
        DPF(DPF_LEVEL_ERROR, ("Init: Failed to allocate memory for m_pDataFormat"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }
    RtlCopyMemory(m_pDataFormat, DataFormat, DataFormat->FormatSize);

    m_KsState = KSSTATE_STOP;
    m_ulMaxBufferSize = LAMA_MAX_STREAM_BUFFER_SIZE; 
    m_ulCurrentBufferSize = 0; 
    m_pAudioBufferMdl = NULL;
    m_pAudioBuffer = NULL;
    m_NotificationEvent = NULL; 

Exit:
    if (!NT_SUCCESS(ntStatus))
    {
        if (m_pPortStream) { m_pPortStream->Release(); m_pPortStream = NULL; }
        if (m_pDataFormat) { ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG); m_pDataFormat = NULL; }
    }
    DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::Init, status=0x%08x", ntStatus));
    return ntStatus;
}

#pragma code_seg("PAGE") 

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::SetFormat - (Existing, no changes needed for this subtask)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::SetFormat
(
    _In_ PKSDATAFORMAT DataFormat
)
{
    PAGED_CODE(); DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::SetFormat")); ASSERT(DataFormat);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    if (m_KsState != KSSTATE_STOP && m_KsState != KSSTATE_ACQUIRE) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Invalid state to set format: %d", m_KsState)); return STATUS_INVALID_DEVICE_STATE; }
    if (!IsEqualGUIDAligned(DataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) || !IsEqualGUIDAligned(DataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) || !IsEqualGUIDAligned(DataFormat->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported DataFormat Major/Sub/Specifier.")); ntStatus = STATUS_NOT_SUPPORTED; goto Exit; }
    PKSDATAFORMAT_WAVEFORMATEX pKsDataFormatWfx = reinterpret_cast<PKSDATAFORMAT_WAVEFORMATEX>(DataFormat); WAVEFORMATEX* wfex = &pKsDataFormatWfx->WaveFormatEx;
    if (wfex->nSamplesPerSec < MIN_SAMPLE_RATE_PCM || wfex->nSamplesPerSec > MAX_SAMPLE_RATE_PCM || (wfex->wFormatTag != WAVE_FORMAT_PCM && wfex->wFormatTag != WAVE_FORMAT_EXTENSIBLE) ) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Sample Rate (%u) or Format Tag (0x%X).", wfex->nSamplesPerSec, wfex->wFormatTag)); ntStatus = STATUS_NOT_SUPPORTED; goto Exit; }
    if (wfex->wBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || wfex->wBitsPerSample > MAX_BITS_PER_SAMPLE_PCM) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Bits Per Sample: %u", wfex->wBitsPerSample)); ntStatus = STATUS_NOT_SUPPORTED; goto Exit; }
    if (wfex->nChannels < MIN_CHANNELS_PCM || wfex->nChannels > MAX_CHANNELS_PCM) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Channel Count: %u", wfex->nChannels)); ntStatus = STATUS_NOT_SUPPORTED; goto Exit; }
    if (m_pDataFormat) { ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG); m_pDataFormat = NULL; } if (m_pAudioBufferMdl) { FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); } 
    m_pDataFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE) ExAllocatePoolWithTag(NonPagedPoolNx, DataFormat->FormatSize, LAMA_POOL_TAG);
    if (!m_pDataFormat) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Failed to allocate memory for new m_pDataFormat")); ntStatus = STATUS_INSUFFICIENT_RESOURCES; goto Exit; }
    RtlCopyMemory(m_pDataFormat, DataFormat, DataFormat->FormatSize); m_ulSampleRate = wfex->nSamplesPerSec; m_ulChannelCount = wfex->nChannels; m_ulBitsPerSample = wfex->wBitsPerSample;
Exit: DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::SetFormat, status=0x%08x", ntStatus)); return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::SetState - (Existing, no changes needed for this subtask)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::SetState(_In_ KSSTATE KsState)
{ PAGED_CODE(); DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::SetState, NewState=%d, CurrentState=%d", KsState, m_KsState)); NTSTATUS ntStatus = STATUS_SUCCESS; KSSTATE oldState = m_KsState; if (KsState == KSSTATE_RUN && oldState != KSSTATE_RUN) { m_ullPlayPosition = 0; m_ullWritePosition = 0; DPF(DPF_LEVEL_INFO, ("SetState: Transitioning to RUN. Data flow should begin.")); } else if (KsState == KSSTATE_STOP && oldState != KSSTATE_STOP) { DPF(DPF_LEVEL_INFO, ("SetState: Transitioning to STOP. Data flow should cease.")); } m_KsState = KsState; DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::SetState, status=0x%08x", ntStatus)); return ntStatus; }

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::GetPosition - (Existing, no changes needed for this subtask)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::GetPosition(_Out_ PKSAUDIO_POSITION Position)
{ ASSERT(Position); if (!m_pPortStream || !m_pDataFormat || m_ulCurrentBufferSize == 0) { Position->PlayOffset = 0; Position->WriteOffset = 0; return (m_pPortStream && m_pDataFormat) ? STATUS_SUCCESS : STATUS_INVALID_DEVICE_STATE; } if (m_KsState == KSSTATE_STOP) { Position->PlayOffset = 0; Position->WriteOffset = 0; return STATUS_SUCCESS; } Position->PlayOffset = m_ullPlayPosition % m_ulCurrentBufferSize; Position->WriteOffset = m_ullWritePosition % m_ulCurrentBufferSize; return STATUS_SUCCESS; }

#pragma code_seg() 
#pragma code_seg("NONPAGED") 

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::AllocateAudioBuffer - (Existing, no changes needed for this subtask)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::AllocateAudioBuffer(_In_ ULONG RequestedSize, _Out_ PMDL *AudioBufferMdl, _Out_ ULONG *ActualSize, _Out_ ULONG *OffsetFromFirstPage, _Out_ MEMORY_CACHING_TYPE *CacheType)
{ ASSERT(ActualSize); ASSERT(AudioBufferMdl); ASSERT(OffsetFromFirstPage); ASSERT(CacheType); if (!m_pPortStream || !m_pDataFormat) return STATUS_INVALID_DEVICE_STATE; NTSTATUS ntStatus = STATUS_SUCCESS; ULONG ulBlockAlign = m_pDataFormat->WaveFormatEx.nBlockAlign; if (ulBlockAlign == 0) ulBlockAlign = (m_ulChannelCount * m_ulBitsPerSample) / 8; if (ulBlockAlign == 0) ulBlockAlign = 1; ULONG ulAdjustedRequestedSize = RequestedSize; if (ulAdjustedRequestedSize % ulBlockAlign != 0) { ulAdjustedRequestedSize = ((ulAdjustedRequestedSize / ulBlockAlign) + 1) * ulBlockAlign; } if (ulAdjustedRequestedSize == 0) { ulAdjustedRequestedSize = (m_ulSampleRate * ulBlockAlign * LAMA_DEFAULT_PACKET_SIZE_MS) / 1000; if (ulAdjustedRequestedSize < ulBlockAlign) ulAdjustedRequestedSize = ulBlockAlign; if (ulAdjustedRequestedSize % ulBlockAlign != 0){ ulAdjustedRequestedSize = ((ulAdjustedRequestedSize / ulBlockAlign) + 1) * ulBlockAlign; } } if (ulAdjustedRequestedSize > m_ulMaxBufferSize) { ulAdjustedRequestedSize = m_ulMaxBufferSize; ulAdjustedRequestedSize -= (ulAdjustedRequestedSize % ulBlockAlign); } if (m_pAudioBufferMdl) { FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); } ntStatus = m_pPortStream->AllocatePagesForMdl(ulAdjustedRequestedSize, AudioBufferMdl, ActualSize, OffsetFromFirstPage); if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("AllocateAudioBuffer: AllocatePagesForMdl failed 0x%x", ntStatus)); m_ulCurrentBufferSize = 0; m_pAudioBufferMdl = NULL; m_pAudioBuffer = NULL; return ntStatus; } m_pAudioBufferMdl = *AudioBufferMdl; m_ulCurrentBufferSize = *ActualSize; if (m_pAudioBufferMdl) { m_pAudioBuffer = MmGetSystemAddressForMdlSafe(m_pAudioBufferMdl, NormalPoolPriority); if (!m_pAudioBuffer) { DPF(DPF_LEVEL_ERROR, ("AllocateAudioBuffer: MmGetSystemAddressForMdlSafe failed.")); m_pPortStream->FreePagesFromMdl(m_pAudioBufferMdl); m_pAudioBufferMdl = NULL; m_ulCurrentBufferSize = 0; return STATUS_INSUFFICIENT_RESOURCES; } } else { m_pAudioBuffer = NULL; } *CacheType = MmCached; m_ullPlayPosition = 0; m_ullWritePosition = 0; DPF(DPF_LEVEL_INFO, ("AllocateAudioBuffer: Req=%u, AdjReq=%u, Actual=%u, VA=0x%p", RequestedSize, ulAdjustedRequestedSize, *ActualSize, m_pAudioBuffer)); return ntStatus; }

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::FreeAudioBuffer - (Existing, no changes needed for this subtask)
//-----------------------------------------------------------------------------
STDMETHODIMP_(VOID)
CMiniportWaveRTLamaLoopbackStream::FreeAudioBuffer(_In_opt_ PMDL AudioBufferMdlParam, _In_ ULONG BufferSizeParam)
{ UNREFERENCED_PARAMETER(BufferSizeParam); m_pAudioBuffer = NULL; PMDL mdlToFree = AudioBufferMdlParam ? AudioBufferMdlParam : m_pAudioBufferMdl; if (mdlToFree) { if (m_pPortStream) { m_pPortStream->FreePagesFromMdl(mdlToFree); } if (mdlToFree == m_pAudioBufferMdl) { m_pAudioBufferMdl = NULL; } } m_ulCurrentBufferSize = 0; }

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::ProcessRenderDataFromWaveRtBuffer
//-----------------------------------------------------------------------------
VOID CMiniportWaveRTLamaLoopbackStream::ProcessRenderDataFromWaveRtBuffer
(
    ULONG ulBufferOffset, 
    ULONG ulByteCount
)
{
    KIRQL oldIrql;
    PLAMA_SHARED_LOOPBACK_BUFFER pCurrentBuffer = &g_InstanceLoopbackBuffers[m_instanceIndex];

    if (m_bCapture) return; 
    if (!pCurrentBuffer->bInitialized || !pCurrentBuffer->pBuffer || !m_pAudioBuffer || ulByteCount == 0)
    {
        DPF(DPF_LEVEL_WARNING, ("ProcessRenderData (Inst %u): Not initialized or no data. SharedInit=%d, SharedBuf=0x%p, WaveRtBuf=0x%p, Count=%u", 
            m_instanceIndex, pCurrentBuffer->bInitialized, pCurrentBuffer->pBuffer, m_pAudioBuffer, ulByteCount));
        return;
    }

    PBYTE pSourceData = (PBYTE)m_pAudioBuffer + ulBufferOffset;
    
    KeAcquireSpinLock(&pCurrentBuffer->SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulWritePointer, 0, 0); 
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulReadPointer, 0, 0);  
    
    ULONG occupiedBytes = currentWritePos - currentReadPos; 
    ULONG freeBytes = pCurrentBuffer->ulBufferSize - occupiedBytes;

    if (ulByteCount > freeBytes)
    {
        DPF(DPF_LEVEL_WARNING, ("ProcessRenderData (Inst %u): Shared buffer overflow. Dropping %u bytes. Free: %u", m_instanceIndex, ulByteCount, freeBytes));
        KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);
        return; 
    }

    ULONG writeIdx = currentWritePos & (pCurrentBuffer->ulBufferSize - 1); 
    ULONG bytesToEndOfBuffer = pCurrentBuffer->ulBufferSize - writeIdx;

    if (ulByteCount <= bytesToEndOfBuffer)
    {
        RtlCopyMemory(pCurrentBuffer->pBuffer + writeIdx, pSourceData, ulByteCount);
    }
    else
    {
        RtlCopyMemory(pCurrentBuffer->pBuffer + writeIdx, pSourceData, bytesToEndOfBuffer);
        RtlCopyMemory(pCurrentBuffer->pBuffer, pSourceData + bytesToEndOfBuffer, ulByteCount - bytesToEndOfBuffer);
    }

    InterlockedExchangeAdd((PLONG)&pCurrentBuffer->ulWritePointer, ulByteCount);
    
    KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);

    m_ullPlayPosition = (m_ullPlayPosition + ulByteCount) % m_ulCurrentBufferSize; 
    m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize; 

    DPF(DPF_LEVEL_TRACE, ("ProcessRenderData (Inst %u): Copied %u bytes to shared buffer. New SharedWritePtr: %u (masked %u)", 
        m_instanceIndex, ulByteCount, pCurrentBuffer->ulWritePointer, pCurrentBuffer->ulWritePointer & (pCurrentBuffer->ulBufferSize-1) ));
}


//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::FetchCaptureDataToWaveRtBuffer
//-----------------------------------------------------------------------------
VOID CMiniportWaveRTLamaLoopbackStream::FetchCaptureDataToWaveRtBuffer
(
    ULONG ulBufferOffset, 
    ULONG ulByteCount
)
{
    KIRQL oldIrql;
    PLAMA_SHARED_LOOPBACK_BUFFER pCurrentBuffer = &g_InstanceLoopbackBuffers[m_instanceIndex];

    if (!m_bCapture) return; 
    if (!pCurrentBuffer->bInitialized || !pCurrentBuffer->pBuffer || !m_pAudioBuffer || ulByteCount == 0)
    {
         DPF(DPF_LEVEL_WARNING, ("FetchCaptureData (Inst %u): Not initialized or no data request. SharedInit=%d, SharedBuf=0x%p, WaveRtBuf=0x%p, Count=%u", 
            m_instanceIndex, pCurrentBuffer->bInitialized, pCurrentBuffer->pBuffer, m_pAudioBuffer, ulByteCount));
        return;
    }
    
    PBYTE pDestData = (PBYTE)m_pAudioBuffer + ulBufferOffset;
    ULONG bytesCopiedFromShared = 0;

    KeAcquireSpinLock(&pCurrentBuffer->SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulWritePointer, 0, 0);
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulReadPointer, 0, 0);
    ULONG availableBytesInShared = currentWritePos - currentReadPos; 

    if (availableBytesInShared > 0)
    {
        bytesCopiedFromShared = min(ulByteCount, availableBytesInShared);
        
        ULONG readIdx = currentReadPos & (pCurrentBuffer->ulBufferSize - 1); 
        ULONG bytesToEndOfBuffer = pCurrentBuffer->ulBufferSize - readIdx;

        if (bytesCopiedFromShared <= bytesToEndOfBuffer)
        {
            RtlCopyMemory(pDestData, pCurrentBuffer->pBuffer + readIdx, bytesCopiedFromShared);
        }
        else
        {
            RtlCopyMemory(pDestData, pCurrentBuffer->pBuffer + readIdx, bytesToEndOfBuffer);
            RtlCopyMemory(pDestData + bytesToEndOfBuffer, pCurrentBuffer->pBuffer, bytesCopiedFromShared - bytesToEndOfBuffer);
        }
        
        InterlockedExchangeAdd((PLONG)&pCurrentBuffer->ulReadPointer, bytesCopiedFromShared);
    }
    
    KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);

    if (bytesCopiedFromShared < ulByteCount)
    {
        RtlZeroMemory(pDestData + bytesCopiedFromShared, ulByteCount - bytesCopiedFromShared);
        DPF(DPF_LEVEL_INFO, ("FetchCaptureData (Inst %u): Shared buffer underrun. Copied %u, Silenced %u bytes.", 
            m_instanceIndex, bytesCopiedFromShared, ulByteCount - bytesCopiedFromShared));
    }

    m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize; 
    m_ullPlayPosition = m_ullWritePosition; 

    DPF(DPF_LEVEL_TRACE, ("FetchCaptureData (Inst %u): Copied %u bytes from shared. New SharedReadPtr: %u (masked %u)", 
        m_instanceIndex, bytesCopiedFromShared, pCurrentBuffer->ulReadPointer, pCurrentBuffer->ulReadPointer & (pCurrentBuffer->ulBufferSize-1) ));
}


//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::NotificationDpcRoutine - (Existing, logic remains similar but would use instance index if it directly accessed shared buffer)
//-----------------------------------------------------------------------------
_Use_decl_annotations_
VOID CMiniportWaveRTLamaLoopbackStream::NotificationDpcRoutine
( PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{ UNREFERENCED_PARAMETER(Dpc); UNREFERENCED_PARAMETER(SystemArgument1); UNREFERENCED_PARAMETER(SystemArgument2); CMiniportWaveRTLamaLoopbackStream *pStream = (CMiniportWaveRTLamaLoopbackStream*)DeferredContext; if (!pStream) return; if (pStream->m_KsState != KSSTATE_RUN) return; ULONG bufferSize = pStream->m_ulCurrentBufferSize; if (bufferSize == 0) return; ULONG notificationInterval = bufferSize / 2; if (!pStream->m_bCapture) { ULONG currentWaveRtPlayOffset = pStream->m_ullPlayPosition % bufferSize; pStream->ProcessRenderDataFromWaveRtBuffer(currentWaveRtPlayOffset, notificationInterval); } else { ULONG currentWaveRtWriteOffset = pStream->m_ullWritePosition % bufferSize; pStream->FetchCaptureDataToWaveRtBuffer(currentWaveRtWriteOffset, notificationInterval); } }

#pragma code_seg("PAGE")
//-----------------------------------------------------------------------------
// Other IMiniportWaveRTStream methods - (Existing, no changes needed for this subtask)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetClock(_Out_ HANDLE *ClockHandle) { PAGED_CODE(); ASSERT(ClockHandle); if (!m_pPortStream) return STATUS_INVALID_DEVICE_STATE; *ClockHandle = m_pPortStream->GetClock(); return STATUS_SUCCESS; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetHwLatency(_Out_ KSRTAUDIO_HWLATENCY *HwLatency) { PAGED_CODE(); ASSERT(HwLatency); HwLatency->FifoSize = m_pDataFormat ? m_pDataFormat->WaveFormatEx.nBlockAlign * 2 : 0; HwLatency->ChipsetDelay = 0; HwLatency->CodecDelay = 0; return STATUS_SUCCESS; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetPositionRegister(_Out_ KSRTAUDIO_HWREGISTER* Register) { PAGED_CODE(); UNREFERENCED_PARAMETER(Register); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetDeviceProperty(_In_ REFGUID rguidPropSet, _In_ ULONG ulPropId, _In_ ULONG ulPropValueSize, _Out_writes_bytes_opt_(ulPropValueSize) PVOID pvPropValue, _Out_ PULONG pulReturnBytes) { PAGED_CODE(); UNREFERENCED_PARAMETER(rguidPropSet); UNREFERENCED_PARAMETER(ulPropId); UNREFERENCED_PARAMETER(ulPropValueSize); UNREFERENCED_PARAMETER(pvPropValue); if(pulReturnBytes) *pulReturnBytes = 0; return STATUS_NOT_SUPPORTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::SetDeviceProperty(_In_ REFGUID rguidPropSet, _In_ ULONG ulPropId, _In_ ULONG ulPropValueSize, _In_reads_bytes_(ulPropValueSize) PVOID pvPropValue) { PAGED_CODE(); UNREFERENCED_PARAMETER(rguidPropSet); UNREFERENCED_PARAMETER(ulPropId); UNREFERENCED_PARAMETER(ulPropValueSize); UNREFERENCED_PARAMETER(pvPropValue); return STATUS_NOT_SUPPORTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetHardwareDescription(_Out_ PENDPOINT_MINIPAIR Minipair, _Outptr_result_maybenull_ PVOID* DeviceContext) { PAGED_CODE(); UNREFERENCED_PARAMETER(Minipair); UNREFERENCED_PARAMETER(DeviceContext); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetSupportedDeviceFormats(_In_ ULONG ulFormatCount, _Out_writes_bytes_opt_(ulFormatCount * sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)) PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFormatArray) { PAGED_CODE(); if (pFormatArray == NULL) { return STATUS_SUCCESS; } if (ulFormatCount == 0) return STATUS_BUFFER_TOO_SMALL; ULONG formatsToCopy = 0; if (ulFormatCount >= 1) { RtlCopyMemory(pFormatArray, &Pcm48000_Stereo_16bit, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)); formatsToCopy++; } if (ulFormatCount >= 2) { RtlCopyMemory(pFormatArray + 1, &Pcm48000_16ch_16bit, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)); formatsToCopy++; } if (ulFormatCount < 2 && formatsToCopy < 2) return STATUS_BUFFER_TOO_SMALL; return STATUS_SUCCESS; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetSupportedDeviceProperties(_In_ ULONG ulPropertyCount, _Out_writes_bytes_opt_(ulPropertyCount * sizeof(KSPROPERTY_ITEM)) PKSPROPERTY_ITEM pPropertyArray) { PAGED_CODE(); UNREFERENCED_PARAMETER(ulPropertyCount); UNREFERENCED_PARAMETER(pPropertyArray); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetSupportedDeviceEvents(_In_ ULONG ulEventCount, _Out_writes_bytes_opt_(ulEventCount * sizeof(KSEVENT_ITEM)) PKSEVENT_ITEM pEventArray) { PAGED_CODE(); UNREFERENCED_PARAMETER(ulEventCount); UNREFERENCED_PARAMETER(pEventArray); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetVolume(_In_ ULONG ulChannel, _Out_ PULONG pulValue) { PAGED_CODE(); UNREFERENCED_PARAMETER(ulChannel); UNREFERENCED_PARAMETER(pulValue); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::SetVolume(_In_ ULONG ulChannel, _In_ ULONG ulValue) { PAGED_CODE(); UNREFERENCED_PARAMETER(ulChannel); UNREFERENCED_PARAMETER(ulValue); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetMute(_Out_ PBOOL pbValue) { PAGED_CODE(); UNREFERENCED_PARAMETER(pbValue); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::SetMute(_In_ BOOL bValue) { PAGED_CODE(); UNREFERENCED_PARAMETER(bValue); return STATUS_NOT_IMPLEMENTED; }

#pragma code_seg("NONPAGED") 
//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::HandleWriteIRPData
//-----------------------------------------------------------------------------
NTSTATUS
CMiniportWaveRTLamaLoopbackStream::HandleWriteIRPData
(
    _In_reads_bytes_(ulByteCount) PVOID pData,
    _In_ ULONG ulByteCount
)
{
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackStream::HandleWriteIRPData] Inst: %u, ByteCount: %u", m_instanceIndex, ulByteCount));
    KIRQL oldIrql;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PLAMA_SHARED_LOOPBACK_BUFFER pCurrentBuffer = &g_InstanceLoopbackBuffers[m_instanceIndex];

    if (!pCurrentBuffer->bInitialized || !pCurrentBuffer->pBuffer)
    {
        DPF(DPF_LEVEL_ERROR, ("HandleWriteIRPData (Inst %u): Shared buffer not initialized.", m_instanceIndex));
        return STATUS_DEVICE_NOT_READY;
    }

    if (pData == NULL && ulByteCount > 0) { DPF(DPF_LEVEL_ERROR, ("HandleWriteIRPData (Inst %u): NULL pData with ulByteCount > 0.", m_instanceIndex)); return STATUS_INVALID_PARAMETER; }
    if (ulByteCount == 0) { DPF(DPF_LEVEL_INFO, ("HandleWriteIRPData (Inst %u): Zero byte count provided.", m_instanceIndex)); return STATUS_SUCCESS; }

    KeAcquireSpinLock(&pCurrentBuffer->SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulWritePointer, 0, 0);
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulReadPointer, 0, 0);
    ULONG occupiedBytes = currentWritePos - currentReadPos;
    ULONG freeBytes = pCurrentBuffer->ulBufferSize - occupiedBytes;

    if (ulByteCount > freeBytes)
    {
        DPF(DPF_LEVEL_ERROR, ("HandleWriteIRPData (Inst %u): Shared buffer overflow. Requested: %u, Free: %u.", m_instanceIndex, ulByteCount, freeBytes));
        ntStatus = STATUS_BUFFER_OVERFLOW; 
        goto Exit;
    }

    ULONG writeIdx = currentWritePos & (pCurrentBuffer->ulBufferSize - 1); 
    ULONG bytesToEndOfBuffer = pCurrentBuffer->ulBufferSize - writeIdx;

    if (ulByteCount <= bytesToEndOfBuffer) { RtlCopyMemory((PBYTE)pCurrentBuffer->pBuffer + writeIdx, pData, ulByteCount); }
    else { RtlCopyMemory((PBYTE)pCurrentBuffer->pBuffer + writeIdx, pData, bytesToEndOfBuffer); RtlCopyMemory((PBYTE)pCurrentBuffer->pBuffer, (PBYTE)pData + bytesToEndOfBuffer, ulByteCount - bytesToEndOfBuffer); }

    InterlockedExchangeAdd((PLONG)&pCurrentBuffer->ulWritePointer, ulByteCount);
    DPF(DPF_LEVEL_INFO, ("HandleWriteIRPData (Inst %u): Wrote %u bytes. New SharedWritePtr: %u", m_instanceIndex, ulByteCount, pCurrentBuffer->ulWritePointer));

Exit:
    KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);
    DPF_LEAVE(("[CMiniportWaveRTLamaLoopbackStream::HandleWriteIRPData] Inst: %u, ntStatus=0x%08x", m_instanceIndex, ntStatus));
    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::HandleReadIRPData
//-----------------------------------------------------------------------------
NTSTATUS
CMiniportWaveRTLamaLoopbackStream::HandleReadIRPData
(
    _Out_writes_bytes_to_(ulReqSize, *pulBytesCopied) PVOID pData,
    _In_ ULONG ulReqSize,
    _Out_ PULONG pulBytesCopied
)
{
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackStream::HandleReadIRPData] Inst: %u, ReqSize: %u", m_instanceIndex, ulReqSize));
    KIRQL oldIrql;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PLAMA_SHARED_LOOPBACK_BUFFER pCurrentBuffer = &g_InstanceLoopbackBuffers[m_instanceIndex];

    ASSERT(pulBytesCopied);
    *pulBytesCopied = 0;

    if (!pCurrentBuffer->bInitialized || !pCurrentBuffer->pBuffer) { DPF(DPF_LEVEL_ERROR, ("HandleReadIRPData (Inst %u): Shared buffer not initialized.", m_instanceIndex)); return STATUS_DEVICE_NOT_READY; }
    if (pData == NULL && ulReqSize > 0) { DPF(DPF_LEVEL_ERROR, ("HandleReadIRPData (Inst %u): NULL pData with ulReqSize > 0.", m_instanceIndex)); return STATUS_INVALID_PARAMETER; }
    if (ulReqSize == 0) { DPF(DPF_LEVEL_INFO, ("HandleReadIRPData (Inst %u): Zero byte request size.", m_instanceIndex)); return STATUS_SUCCESS; }

    KeAcquireSpinLock(&pCurrentBuffer->SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulWritePointer, 0, 0);
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulReadPointer, 0, 0);
    ULONG availableDataInShared = currentWritePos - currentReadPos;
    ULONG bytesToCopy = 0;

    if (availableDataInShared > 0)
    {
        bytesToCopy = min(ulReqSize, availableDataInShared);
        ULONG readIdx = currentReadPos & (pCurrentBuffer->ulBufferSize - 1); 
        ULONG bytesToEndOfBuffer = pCurrentBuffer->ulBufferSize - readIdx;
        if (bytesToCopy <= bytesToEndOfBuffer) { RtlCopyMemory(pData, (PBYTE)pCurrentBuffer->pBuffer + readIdx, bytesToCopy); }
        else { RtlCopyMemory(pData, (PBYTE)pCurrentBuffer->pBuffer + readIdx, bytesToEndOfBuffer); RtlCopyMemory((PBYTE)pData + bytesToEndOfBuffer, (PBYTE)pCurrentBuffer->pBuffer, bytesToCopy - bytesToEndOfBuffer); }
        InterlockedExchangeAdd((PLONG)&pCurrentBuffer->ulReadPointer, bytesToCopy);
        *pulBytesCopied = bytesToCopy;
        DPF(DPF_LEVEL_INFO, ("HandleReadIRPData (Inst %u): Read %u bytes. New SharedReadPtr: %u", m_instanceIndex, bytesToCopy, pCurrentBuffer->ulReadPointer));
    } else { DPF(DPF_LEVEL_INFO, ("HandleReadIRPData (Inst %u): No data in shared buffer.",m_instanceIndex)); }
    
    KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);
    DPF_LEAVE(("[CMiniportWaveRTLamaLoopbackStream::HandleReadIRPData] Inst: %u, ntStatus=0x%08x, BytesCopied=%u", m_instanceIndex, ntStatus, *pulBytesCopied));
    return ntStatus;
}
#pragma code_seg() 

// End of hdmitopo.cpp
