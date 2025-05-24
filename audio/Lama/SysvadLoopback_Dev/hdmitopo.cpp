#include "lamaloopbackstream.h"
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
  m_pAudioBufferMdl(NULL),
  m_pAudioBuffer(NULL),      
  m_ulCurrentBufferSize(0),
  m_ulMaxBufferSize(LAMA_MAX_STREAM_BUFFER_SIZE), // Corrected from LAMA_MAX_BUFFER_SIZE
  m_ulChannelCount(0),
  m_ulSampleRate(0),
  m_ulBitsPerSample(0),
  m_NotificationEvent(NULL),
  m_ulDmaMovementRate(0), // Initialize added members
  m_ullPlayPosition(0),
  m_ullWritePosition(0)
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::CMiniportWaveRTLamaLoopbackStream"));
    RtlZeroMemory(&m_NotificationTimer, sizeof(m_NotificationTimer)); 
    RtlZeroMemory(&m_NotificationDpc, sizeof(m_NotificationDpc)); // Initialize DPC object
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
        // If this driver created/duplicated the event, it should close it.
        // If obtained from PortCls (e.g. GetNotificationEvent), PortCls manages it.
        // ObCloseHandle(m_NotificationEvent, KernelMode); // Example
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
    _In_ BOOLEAN Capture
)
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::Init"));

    ASSERT(PortStream);
    ASSERT(DataFormat);

    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_pPortStream = PortStream;
    m_pPortStream->AddRef();
    m_bCapture = Capture;

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
    m_NotificationEvent = NULL; // Must be acquired from PortCls

    // Example: Initialize DPC for notifications (if this stream uses it)
    // KeInitializeDpc(&m_NotificationDpc, NotificationDpcRoutine, this);

Exit:
    if (!NT_SUCCESS(ntStatus))
    {
        if (m_pPortStream) { m_pPortStream->Release(); m_pPortStream = NULL; }
        if (m_pDataFormat) { ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG); m_pDataFormat = NULL; }
    }
    DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::Init, status=0x%08x", ntStatus));
    return ntStatus;
}

// Implementations for SetFormat, SetState, GetPosition, AllocateAudioBuffer, FreeAudioBuffer, GetClock, GetHwLatency
// from previous turn are assumed to be correct and are omitted here for brevity,
// unless they need specific changes for packet handling logic.

#pragma code_seg("PAGE") // Ensure paged code for methods that allow it

// ... (Previously implemented SetFormat, SetState, GetPosition etc. - assumed to be mostly correct) ...
// For brevity, only showing the new methods and relevant parts of existing methods.

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::SetFormat
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::SetFormat
(
    _In_ PKSDATAFORMAT DataFormat
)
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::SetFormat"));
    ASSERT(DataFormat);

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (m_KsState != KSSTATE_STOP && m_KsState != KSSTATE_ACQUIRE) 
    {
        DPF(DPF_LEVEL_ERROR, ("SetFormat: Invalid state to set format: %d", m_KsState));
        return STATUS_INVALID_DEVICE_STATE; 
    }

    if (!IsEqualGUIDAligned(DataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(DataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(DataFormat->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported DataFormat Major/Sub/Specifier."));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    
    PKSDATAFORMAT_WAVEFORMATEX pKsDataFormatWfx = reinterpret_cast<PKSDATAFORMAT_WAVEFORMATEX>(DataFormat);
    WAVEFORMATEX* wfex = &pKsDataFormatWfx->WaveFormatEx;

    if (wfex->nSamplesPerSec < MIN_SAMPLE_RATE_PCM || wfex->nSamplesPerSec > MAX_SAMPLE_RATE_PCM ||
        (wfex->wFormatTag != WAVE_FORMAT_PCM && wfex->wFormatTag != WAVE_FORMAT_EXTENSIBLE) )
    {
        DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Sample Rate (%u) or Format Tag (0x%X).", wfex->nSamplesPerSec, wfex->wFormatTag));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    if (wfex->wBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || wfex->wBitsPerSample > MAX_BITS_PER_SAMPLE_PCM)
    {
        DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Bits Per Sample: %u", wfex->wBitsPerSample));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    if (wfex->nChannels < MIN_CHANNELS_PCM || wfex->nChannels > MAX_CHANNELS_PCM)
    {
        DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Channel Count: %u", wfex->nChannels));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }

    if (m_pDataFormat) { ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG); m_pDataFormat = NULL; }
    if (m_pAudioBufferMdl) { FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); } // Free buffer if format changes

    m_pDataFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE) ExAllocatePoolWithTag(
                        NonPagedPoolNx, DataFormat->FormatSize, LAMA_POOL_TAG);
    if (!m_pDataFormat)
    {
        DPF(DPF_LEVEL_ERROR, ("SetFormat: Failed to allocate memory for new m_pDataFormat"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }
    RtlCopyMemory(m_pDataFormat, DataFormat, DataFormat->FormatSize);

    m_ulSampleRate    = wfex->nSamplesPerSec;
    m_ulChannelCount  = wfex->nChannels;
    m_ulBitsPerSample = wfex->wBitsPerSample;

Exit:
    DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::SetFormat, status=0x%08x", ntStatus));
    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::SetState
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::SetState
(
    _In_ KSSTATE KsState
)
{
    PAGED_CODE();
    DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::SetState, NewState=%d, CurrentState=%d", KsState, m_KsState));

    NTSTATUS ntStatus = STATUS_SUCCESS;
    KSSTATE oldState = m_KsState;
    
    // TODO: Add specific logic for state transitions, e.g., starting/stopping DMA simulation or timers.
    // For WaveRT, when transitioning to RUN, this is where the miniport often starts its notification mechanism
    // (e.g., by calling IPortWaveRTStream::NotifyLoop).

    if (KsState == KSSTATE_RUN && oldState != KSSTATE_RUN)
    {
        // Reset positions or start timers/DPCs if applicable for data flow simulation
        m_ullPlayPosition = 0;
        m_ullWritePosition = 0;
        // If using a DPC for data transfer, might arm a timer here or wait for initial Notify from PortCls
        DPF(DPF_LEVEL_INFO, ("SetState: Transitioning to RUN. Data flow should begin."));
    }
    else if (KsState == KSSTATE_STOP && oldState != KSSTATE_STOP)
    {
        DPF(DPF_LEVEL_INFO, ("SetState: Transitioning to STOP. Data flow should cease."));
        // Cancel timers, etc.
    }
    
    m_KsState = KsState;
    DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::SetState, status=0x%08x", ntStatus));
    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::GetPosition
// (Non-paged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::GetPosition
(
    _Out_ PKSAUDIO_POSITION Position
)
{
    ASSERT(Position);
    if (!m_pPortStream || !m_pDataFormat || m_ulCurrentBufferSize == 0)
    {
        Position->PlayOffset = 0;
        Position->WriteOffset = 0;
        return  (m_pPortStream && m_pDataFormat) ? STATUS_SUCCESS : STATUS_INVALID_DEVICE_STATE;
    }

    if (m_KsState == KSSTATE_STOP)
    {
        Position->PlayOffset = 0;
        Position->WriteOffset = 0;
        return STATUS_SUCCESS;
    }

    // This is a simplified simulation. Real WaveRT position is complex.
    // It relies on IPortWaveRTStream::GetPacketCount or hardware DMA counters.
    // For loopback, we simulate based on data flow through shared buffer.
    // Let's use m_ullPlayPosition as the "hardware" play position for this stream's WaveRT buffer.
    // And m_ullWritePosition as where the client can write (render) or where driver writes (capture).
    
    Position->PlayOffset = m_ullPlayPosition % m_ulCurrentBufferSize;
    Position->WriteOffset = m_ullWritePosition % m_ulCurrentBufferSize;

    return STATUS_SUCCESS;
}

#pragma code_seg() // End PAGED_CODE for SetState, etc.
#pragma code_seg("NONPAGED") // Begin NONPAGED for buffer/DMA related methods

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::AllocateAudioBuffer
// (Non-paged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::AllocateAudioBuffer
(
    _In_  ULONG RequestedSize,
    _Out_ PMDL  *AudioBufferMdl,           
    _Out_ ULONG *ActualSize,               
    _Out_ ULONG *OffsetFromFirstPage,      
    _Out_ MEMORY_CACHING_TYPE *CacheType  
)
{
    ASSERT(ActualSize); ASSERT(AudioBufferMdl); ASSERT(OffsetFromFirstPage); ASSERT(CacheType);

    if (!m_pPortStream || !m_pDataFormat) return STATUS_INVALID_DEVICE_STATE;

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulBlockAlign = m_pDataFormat->WaveFormatEx.nBlockAlign;
    if (ulBlockAlign == 0) ulBlockAlign = (m_ulChannelCount * m_ulBitsPerSample) / 8;
    if (ulBlockAlign == 0) ulBlockAlign = 1; 

    ULONG ulAdjustedRequestedSize = RequestedSize;
    if (ulAdjustedRequestedSize % ulBlockAlign != 0) {
        ulAdjustedRequestedSize = ((ulAdjustedRequestedSize / ulBlockAlign) + 1) * ulBlockAlign;
    }
    if (ulAdjustedRequestedSize == 0) {
        ulAdjustedRequestedSize = (m_ulSampleRate * ulBlockAlign * LAMA_DEFAULT_PACKET_SIZE_MS) / 1000;
        if (ulAdjustedRequestedSize < ulBlockAlign) ulAdjustedRequestedSize = ulBlockAlign;
        if (ulAdjustedRequestedSize % ulBlockAlign != 0){
             ulAdjustedRequestedSize = ((ulAdjustedRequestedSize / ulBlockAlign) + 1) * ulBlockAlign;
        }
    }
    if (ulAdjustedRequestedSize > m_ulMaxBufferSize) {
        ulAdjustedRequestedSize = m_ulMaxBufferSize;
        ulAdjustedRequestedSize -= (ulAdjustedRequestedSize % ulBlockAlign);
    }
    
    if (m_pAudioBufferMdl) { FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); }

    ntStatus = m_pPortStream->AllocatePagesForMdl(ulAdjustedRequestedSize, AudioBufferMdl, ActualSize, OffsetFromFirstPage);

    if (!NT_SUCCESS(ntStatus)) {
        DPF(DPF_LEVEL_ERROR, ("AllocateAudioBuffer: AllocatePagesForMdl failed 0x%x", ntStatus));
        m_ulCurrentBufferSize = 0; m_pAudioBufferMdl = NULL; m_pAudioBuffer = NULL;
        return ntStatus;
    }

    m_pAudioBufferMdl = *AudioBufferMdl;
    m_ulCurrentBufferSize = *ActualSize; 

    if (m_pAudioBufferMdl) {
        m_pAudioBuffer = MmGetSystemAddressForMdlSafe(m_pAudioBufferMdl, NormalPoolPriority);
        if (!m_pAudioBuffer) {
            DPF(DPF_LEVEL_ERROR, ("AllocateAudioBuffer: MmGetSystemAddressForMdlSafe failed."));
            m_pPortStream->FreePagesFromMdl(m_pAudioBufferMdl);
            m_pAudioBufferMdl = NULL; m_ulCurrentBufferSize = 0;
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    } else { m_pAudioBuffer = NULL; } // Should not happen
    
    *CacheType = MmCached; 
    m_ullPlayPosition = 0; // Reset stream's internal position counters
    m_ullWritePosition = 0; // Typically client writes at offset 0 to start.

    // Example: Get notification event from PortCls (if not already done or if it changes per buffer)
    // m_NotificationEvent = m_pPortStream->GetNotificationEvent();
    // This is usually done once after stream creation or in Init.

    DPF(DPF_LEVEL_INFO, ("AllocateAudioBuffer: Req=%u, AdjReq=%u, Actual=%u, VA=0x%p", 
        RequestedSize, ulAdjustedRequestedSize, *ActualSize, m_pAudioBuffer));
    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::FreeAudioBuffer
// (Non-paged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(VOID)
CMiniportWaveRTLamaLoopbackStream::FreeAudioBuffer
(
    _In_opt_ PMDL AudioBufferMdlParam, 
    _In_ ULONG BufferSizeParam 
)
{
    UNREFERENCED_PARAMETER(BufferSizeParam); 
    m_pAudioBuffer = NULL; 

    PMDL mdlToFree = AudioBufferMdlParam ? AudioBufferMdlParam : m_pAudioBufferMdl;

    if (mdlToFree) {
        if (m_pPortStream) { m_pPortStream->FreePagesFromMdl(mdlToFree); }
        if (mdlToFree == m_pAudioBufferMdl) { m_pAudioBufferMdl = NULL; }
    }
    m_ulCurrentBufferSize = 0;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::ProcessRenderDataFromWaveRtBuffer
// This function is called when new data is available in this stream's WaveRT buffer (m_pAudioBuffer)
// and needs to be written to the global shared loopback buffer.
// This would typically be called from a DPC or a worker thread that handles WaveRT notifications.
// Parameters:
//   ulBufferOffset - Offset in m_pAudioBuffer where the new data starts.
//   ulByteCount    - Number of bytes of new data in m_pAudioBuffer.
//-----------------------------------------------------------------------------
VOID CMiniportWaveRTLamaLoopbackStream::ProcessRenderDataFromWaveRtBuffer
(
    ULONG ulBufferOffset, 
    ULONG ulByteCount
)
{
    KIRQL oldIrql;

    if (m_bCapture) return; // This is for render streams only
    if (!g_SharedLoopbackBuffer.bInitialized || !g_SharedLoopbackBuffer.pBuffer || !m_pAudioBuffer || ulByteCount == 0)
    {
        DPF(DPF_LEVEL_WARNING, ("ProcessRenderData: Not initialized or no data. SharedInit=%d, SharedBuf=0x%p, WaveRtBuf=0x%p, Count=%u", 
            g_SharedLoopbackBuffer.bInitialized, g_SharedLoopbackBuffer.pBuffer, m_pAudioBuffer, ulByteCount));
        return;
    }

    PBYTE pSourceData = (PBYTE)m_pAudioBuffer + ulBufferOffset;
    
    KeAcquireSpinLock(&g_SharedLoopbackBuffer.SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&g_SharedLoopbackBuffer.ulWritePointer, 0, 0); // Read current volatile value
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&g_SharedLoopbackBuffer.ulReadPointer, 0, 0);  // Read current volatile value
    
    // Calculate free space in shared buffer. If write catches up to read, buffer is full.
    // (currentReadPos - currentWritePos - 1 + ulBufferSize) % ulBufferSize;
    // More simply: Total size - occupied size. Occupied = write - read.
    ULONG occupiedBytes = currentWritePos - currentReadPos; // Assuming running counters
    ULONG freeBytes = g_SharedLoopbackBuffer.ulBufferSize - occupiedBytes;

    if (ulByteCount > freeBytes)
    {
        // Shared buffer overflow. Data will be lost. Or handle differently (e.g. wait, overwrite oldest)
        // For this simple loopback, we might just drop new data or overwrite.
        // Let's choose to drop for now if not enough contiguous space for this whole packet.
        DPF(DPF_LEVEL_WARNING, ("ProcessRenderData: Shared buffer overflow. Dropping %u bytes. Free: %u", ulByteCount, freeBytes));
        KeReleaseSpinLock(&g_SharedLoopbackBuffer.SpinLock, oldIrql);
        return; // Or, could try to write part of it if desired.
    }

    // Perform the copy to the shared ring buffer
    ULONG writeIdx = currentWritePos & (g_SharedLoopbackBuffer.ulBufferSize - 1); // Mask for current index
    ULONG bytesToEndOfBuffer = g_SharedLoopbackBuffer.ulBufferSize - writeIdx;

    if (ulByteCount <= bytesToEndOfBuffer)
    {
        RtlCopyMemory(g_SharedLoopbackBuffer.pBuffer + writeIdx, pSourceData, ulByteCount);
    }
    else
    {
        // Data wraps around the end of the shared buffer
        RtlCopyMemory(g_SharedLoopbackBuffer.pBuffer + writeIdx, pSourceData, bytesToEndOfBuffer);
        RtlCopyMemory(g_SharedLoopbackBuffer.pBuffer, pSourceData + bytesToEndOfBuffer, ulByteCount - bytesToEndOfBuffer);
    }

    // Atomically update the write pointer (as a running total)
    InterlockedExchangeAdd((PLONG)&g_SharedLoopbackBuffer.ulWritePointer, ulByteCount);
    
    KeReleaseSpinLock(&g_SharedLoopbackBuffer.SpinLock, oldIrql);

    // Update this stream's internal write position (which is play position for render stream in WaveRT buffer)
    m_ullPlayPosition = (m_ullPlayPosition + ulByteCount) % m_ulCurrentBufferSize; // For GetPosition
    m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize; // Client writes here

    DPF(DPF_LEVEL_TRACE, ("ProcessRenderData: Copied %u bytes to shared buffer. New SharedWritePtr: %u (masked %u)", 
        ulByteCount, g_SharedLoopbackBuffer.ulWritePointer, g_SharedLoopbackBuffer.ulWritePointer & (g_SharedLoopbackBuffer.ulBufferSize-1) ));

    // TODO: Signal an event if capture streams might be waiting for data.
    // This depends on the overall synchronization mechanism.
}


//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::FetchCaptureDataToWaveRtBuffer
// This function is called when the capture stream needs to provide data in its WaveRT buffer (m_pAudioBuffer),
// by reading from the global shared loopback buffer.
// This would typically be called from a DPC or a worker thread that handles WaveRT notifications.
// Parameters:
//   ulBufferOffset - Offset in m_pAudioBuffer where the captured data should be written.
//   ulByteCount    - Number of bytes to fill in m_pAudioBuffer.
//-----------------------------------------------------------------------------
VOID CMiniportWaveRTLamaLoopbackStream::FetchCaptureDataToWaveRtBuffer
(
    ULONG ulBufferOffset, 
    ULONG ulByteCount
)
{
    KIRQL oldIrql;

    if (!m_bCapture) return; // This is for capture streams only
    if (!g_SharedLoopbackBuffer.bInitialized || !g_SharedLoopbackBuffer.pBuffer || !m_pAudioBuffer || ulByteCount == 0)
    {
         DPF(DPF_LEVEL_WARNING, ("FetchCaptureData: Not initialized or no data request. SharedInit=%d, SharedBuf=0x%p, WaveRtBuf=0x%p, Count=%u", 
            g_SharedLoopbackBuffer.bInitialized, g_SharedLoopbackBuffer.pBuffer, m_pAudioBuffer, ulByteCount));
        return;
    }
    
    PBYTE pDestData = (PBYTE)m_pAudioBuffer + ulBufferOffset;
    ULONG bytesCopiedFromShared = 0;

    KeAcquireSpinLock(&g_SharedLoopbackBuffer.SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&g_SharedLoopbackBuffer.ulWritePointer, 0, 0);
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&g_SharedLoopbackBuffer.ulReadPointer, 0, 0);
    ULONG availableBytesInShared = currentWritePos - currentReadPos; // Assuming running counters

    if (availableBytesInShared > 0)
    {
        bytesCopiedFromShared = min(ulByteCount, availableBytesInShared);
        
        ULONG readIdx = currentReadPos & (g_SharedLoopbackBuffer.ulBufferSize - 1); // Mask for current index
        ULONG bytesToEndOfBuffer = g_SharedLoopbackBuffer.ulBufferSize - readIdx;

        if (bytesCopiedFromShared <= bytesToEndOfBuffer)
        {
            RtlCopyMemory(pDestData, g_SharedLoopbackBuffer.pBuffer + readIdx, bytesCopiedFromShared);
        }
        else
        {
            // Data wraps around the end of the shared buffer
            RtlCopyMemory(pDestData, g_SharedLoopbackBuffer.pBuffer + readIdx, bytesToEndOfBuffer);
            RtlCopyMemory(pDestData + bytesToEndOfBuffer, g_SharedLoopbackBuffer.pBuffer, bytesCopiedFromShared - bytesToEndOfBuffer);
        }
        
        // Atomically update the read pointer (as a running total)
        InterlockedExchangeAdd((PLONG)&g_SharedLoopbackBuffer.ulReadPointer, bytesCopiedFromShared);
    }
    
    KeReleaseSpinLock(&g_SharedLoopbackBuffer.SpinLock, oldIrql);

    // If not enough data was available in shared buffer, fill remaining with silence
    if (bytesCopiedFromShared < ulByteCount)
    {
        RtlZeroMemory(pDestData + bytesCopiedFromShared, ulByteCount - bytesCopiedFromShared);
        DPF(DPF_LEVEL_INFO, ("FetchCaptureData: Shared buffer underrun. Copied %u, Silenced %u bytes.", 
            bytesCopiedFromShared, ulByteCount - bytesCopiedFromShared));
    }

    // Update this stream's internal position counters (for GetPosition)
    // For capture, PlayPosition is where data becomes available for client, WritePosition is where driver writes.
    m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize; // Driver wrote here
    m_ullPlayPosition = m_ullWritePosition; // Data is immediately "playable" after driver writes it

    DPF(DPF_LEVEL_TRACE, ("FetchCaptureData: Copied %u bytes from shared. New SharedReadPtr: %u (masked %u)", 
        bytesCopiedFromShared, g_SharedLoopbackBuffer.ulReadPointer, g_SharedLoopbackBuffer.ulReadPointer & (g_SharedLoopbackBuffer.ulBufferSize-1) ));

    // TODO: Signal render stream if shared buffer was full and now has space? (less common)
}


//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::NotificationDpcRoutine (Example DPC)
// This is a static method. The actual instance is passed in DeferredContext.
//-----------------------------------------------------------------------------
_Use_decl_annotations_
VOID CMiniportWaveRTLamaLoopbackStream::NotificationDpcRoutine
(
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2
)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    CMiniportWaveRTLamaLoopbackStream *pStream = (CMiniportWaveRTLamaLoopbackStream*)DeferredContext;
    if (!pStream) return;

    // This DPC is triggered by PortCls after it calls IPortWaveRTStream::Notify()
    // or when the registered notification event is signaled.
    // The miniport needs to determine how much data was processed or needs to be processed.

    if (pStream->m_KsState != KSSTATE_RUN) return; // Only process if running

    ULONG bufferSize = pStream->m_ulCurrentBufferSize;
    if (bufferSize == 0) return;

    // For WaveRT, client provides two notification points in the buffer (typically 0 and bufferSize/2).
    // The miniport gets the byte count processed by the client (render) or filled by DMA (capture)
    // from IPortWaveRTStream::GetAvailableByteCount() or similar.
    // Or, for event-driven, the event signals every time client crosses a notification point.
    
    // Let's assume a notification interval (e.g. half the buffer).
    // This is a simplified model. Actual WaveRT notification handling is more complex
    // and involves IPortWaveRTStream methods to get exact buffer positions and counts.
    ULONG notificationInterval = bufferSize / 2; // Example for 2 notifications per buffer cycle
    
    if (!pStream->m_bCapture) // Render Stream
    {
        // Client has written 'notificationInterval' data into m_pAudioBuffer.
        // The exact offset and amount should be determined via IPortWaveRTStream methods.
        // For this example, assume we process 'notificationInterval' data from current WaveRT play position.
        ULONG currentWaveRtPlayOffset = pStream->m_ullPlayPosition % bufferSize; 
        pStream->ProcessRenderDataFromWaveRtBuffer(currentWaveRtPlayOffset, notificationInterval);
    }
    else // Capture Stream
    {
        // We need to fill 'notificationInterval' data into m_pAudioBuffer.
        // The exact offset and amount should be determined via IPortWaveRTStream methods.
        // For this example, assume we fill 'notificationInterval' data at current WaveRT write position.
        ULONG currentWaveRtWriteOffset = pStream->m_ullWritePosition % bufferSize;
        pStream->FetchCaptureDataToWaveRtBuffer(currentWaveRtWriteOffset, notificationInterval);
    }

    // Re-arm notification if necessary, or rely on PortCls to call NotifyLoop.
    // If using KeSetTimerEx, re-arm it here.
}


// Stubs for other methods from previous turn...
#pragma code_seg("PAGE")

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::GetClock(_Out_ HANDLE *ClockHandle)
{
    PAGED_CODE(); ASSERT(ClockHandle);
    if (!m_pPortStream) return STATUS_INVALID_DEVICE_STATE;
    *ClockHandle = m_pPortStream->GetClock();
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::GetHwLatency(_Out_ KSRTAUDIO_HWLATENCY *HwLatency)
{
    PAGED_CODE(); ASSERT(HwLatency);
    HwLatency->FifoSize = m_pDataFormat ? m_pDataFormat->WaveFormatEx.nBlockAlign * 2 : 0;
    HwLatency->ChipsetDelay = 0; HwLatency->CodecDelay = 0;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetPositionRegister(_Out_ KSRTAUDIO_HWREGISTER* Register)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(Register); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetDeviceProperty(_In_ REFGUID rguidPropSet, _In_ ULONG ulPropId, _In_ ULONG ulPropValueSize, _Out_writes_bytes_opt_(ulPropValueSize) PVOID pvPropValue, _Out_ PULONG pulReturnBytes)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(rguidPropSet); UNREFERENCED_PARAMETER(ulPropId); UNREFERENCED_PARAMETER(ulPropValueSize); UNREFERENCED_PARAMETER(pvPropValue); if(pulReturnBytes) *pulReturnBytes = 0; return STATUS_NOT_SUPPORTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::SetDeviceProperty(_In_ REFGUID rguidPropSet, _In_ ULONG ulPropId, _In_ ULONG ulPropValueSize, _In_reads_bytes_(ulPropValueSize) PVOID pvPropValue)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(rguidPropSet); UNREFERENCED_PARAMETER(ulPropId); UNREFERENCED_PARAMETER(ulPropValueSize); UNREFERENCED_PARAMETER(pvPropValue); return STATUS_NOT_SUPPORTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetHardwareDescription(_Out_ PENDPOINT_MINIPAIR Minipair, _Outptr_result_maybenull_ PVOID* DeviceContext)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(Minipair); UNREFERENCED_PARAMETER(DeviceContext); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetSupportedDeviceFormats(_In_ ULONG ulFormatCount, _Out_writes_bytes_opt_(ulFormatCount * sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)) PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFormatArray)
{
    PAGED_CODE(); 
    ULONG pulFormatCountLocal; // Shadow outer scope variable if any
    if (pFormatArray == NULL) {
        pulFormatCountLocal = 2; // Pcm48000_Stereo_16bit and Pcm48000_16ch_16bit
        // If the caller provided a PULONG to store the count, use it if available from the signature.
        // The IMiniportWaveRTStream::GetSupportedDeviceFormats does not have PULONG pulFormatCount as output.
        // This is a common pattern for other GetXxx(count, array) methods though.
        // For this DDI, the count is implicit. If buffer is too small, it might return STATUS_BUFFER_TOO_SMALL.
        // Or it fills what it can and returns STATUS_SUCCESS.
        // The subtask asks for this method, but its signature in IMiniportWaveRTStream is fixed.
        // Let's assume the caller knows how many to expect or calls with various ulFormatCount.
        return STATUS_SUCCESS; // Indicate success, caller can try with larger ulFormatCount if needed.
    }
    if (ulFormatCount == 0) return STATUS_BUFFER_TOO_SMALL;
    
    ULONG formatsToCopy = 0;
    if (ulFormatCount >= 1) {
        RtlCopyMemory(pFormatArray, &Pcm48000_Stereo_16bit, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
        formatsToCopy++;
    }
    if (ulFormatCount >= 2) {
        RtlCopyMemory(pFormatArray + 1, &Pcm48000_16ch_16bit, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
        formatsToCopy++;
    }
    // This method doesn't officially return the number of formats copied via an out param.
    // It returns STATUS_BUFFER_OVERFLOW if ulFormatCount is too small for all data.
    // Or STATUS_SUCCESS if it copied all or some.
    if (ulFormatCount < 2 && formatsToCopy < 2) return STATUS_BUFFER_TOO_SMALL; // Example: if we insist on returning all
    return STATUS_SUCCESS;
}
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetSupportedDeviceProperties(_In_ ULONG ulPropertyCount, _Out_writes_bytes_opt_(ulPropertyCount * sizeof(KSPROPERTY_ITEM)) PKSPROPERTY_ITEM pPropertyArray)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(ulPropertyCount); UNREFERENCED_PARAMETER(pPropertyArray); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetSupportedDeviceEvents(_In_ ULONG ulEventCount, _Out_writes_bytes_opt_(ulEventCount * sizeof(KSEVENT_ITEM)) PKSEVENT_ITEM pEventArray)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(ulEventCount); UNREFERENCED_PARAMETER(pEventArray); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetVolume(_In_ ULONG ulChannel, _Out_ PULONG pulValue)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(ulChannel); UNREFERENCED_PARAMETER(pulValue); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::SetVolume(_In_ ULONG ulChannel, _In_ ULONG ulValue)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(ulChannel); UNREFERENCED_PARAMETER(ulValue); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::GetMute(_Out_ PBOOL pbValue)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(pbValue); return STATUS_NOT_IMPLEMENTED; }
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackStream::SetMute(_In_ BOOL bValue)
{ PAGED_CODE(); UNREFERENCED_PARAMETER(bValue); return STATUS_NOT_IMPLEMENTED; }

#pragma code_seg() // End NONPAGED / PAGED sections

// End of lamaloopbackstream.cpp
