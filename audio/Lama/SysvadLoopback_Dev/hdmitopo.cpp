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
  m_ulChannelCount(0),    // This will store the client's channel count
  m_ulSampleRate(0),
  m_ulBitsPerSample(0),   // This will store the client's bits per sample
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

    // Store client's format details
    m_ulSampleRate    = wfex->nSamplesPerSec;
    m_ulChannelCount  = wfex->nChannels;
    m_ulBitsPerSample = wfex->wBitsPerSample;

    DPF(DPF_LEVEL_INFO, ("Stream Init (Inst %u, %s): Client Format %uHz, %uch, %ubit",
        m_instanceIndex, m_bCapture ? "Capture" : "Render", m_ulSampleRate, m_ulChannelCount, m_ulBitsPerSample));


    if (m_ulSampleRate < MIN_SAMPLE_RATE_PCM || m_ulSampleRate > MAX_SAMPLE_RATE_PCM ||
        (wfex->wFormatTag != WAVE_FORMAT_PCM && wfex->wFormatTag != WAVE_FORMAT_EXTENSIBLE) )
    {
        DPF(DPF_LEVEL_ERROR, ("Init: Unsupported Sample Rate (%u) or Format Tag (0x%X).", m_ulSampleRate, wfex->wFormatTag));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    // For WaveRT, we'll assume 16-bit PCM as the internal shared buffer is 16-bit.
    // If client requests other bit depths, that's an advanced scenario needing conversion.
    // The current common.h also defines MIN/MAX_BITS_PER_SAMPLE_PCM as 16.
    if (m_ulBitsPerSample != 16)
    {
        DPF(DPF_LEVEL_ERROR, ("Init: WaveRT Stream currently only supports 16 Bits Per Sample, got %u", m_ulBitsPerSample));
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
    if (m_ulChannelCount < MIN_CHANNELS_PCM || m_ulChannelCount > MAX_CHANNELS_PCM) // Client can request up to 16
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
// CMiniportWaveRTLamaLoopbackStream::SetFormat - (Unchanged for Strategy A)
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

    if (wfex->wBitsPerSample != 16) { DPF(DPF_LEVEL_ERROR, ("SetFormat: WaveRT Stream currently only supports 16 Bits Per Sample: %u", wfex->wBitsPerSample)); ntStatus = STATUS_NOT_SUPPORTED; goto Exit; }
    if (wfex->nChannels < MIN_CHANNELS_PCM || wfex->nChannels > MAX_CHANNELS_PCM) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Unsupported Channel Count: %u", wfex->nChannels)); ntStatus = STATUS_NOT_SUPPORTED; goto Exit; }

    if (m_pDataFormat) { ExFreePoolWithTag(m_pDataFormat, LAMA_POOL_TAG); m_pDataFormat = NULL; }
    if (m_pAudioBufferMdl) { FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); /* Resets m_pAudioBufferMdl, m_pAudioBuffer, m_ulCurrentBufferSize */ }
    m_pDataFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE) ExAllocatePoolWithTag(NonPagedPoolNx, DataFormat->FormatSize, LAMA_POOL_TAG);
    if (!m_pDataFormat) { DPF(DPF_LEVEL_ERROR, ("SetFormat: Failed to allocate memory for new m_pDataFormat")); ntStatus = STATUS_INSUFFICIENT_RESOURCES; goto Exit; }
    RtlCopyMemory(m_pDataFormat, DataFormat, DataFormat->FormatSize);
    // Update client's format
    m_ulSampleRate = wfex->nSamplesPerSec;
    m_ulChannelCount = wfex->nChannels;
    m_ulBitsPerSample = wfex->wBitsPerSample;
    DPF(DPF_LEVEL_INFO, ("Stream SetFormat (Inst %u, %s): Client Format %uHz, %uch, %ubit",
        m_instanceIndex, m_bCapture ? "Capture" : "Render", m_ulSampleRate, m_ulChannelCount, m_ulBitsPerSample));

Exit: DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::SetFormat, status=0x%08x", ntStatus)); return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::SetState - (Unchanged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::SetState(_In_ KSSTATE KsState)
{ PAGED_CODE(); DPF_ENTER(("CMiniportWaveRTLamaLoopbackStream::SetState, NewState=%d, CurrentState=%d", KsState, m_KsState)); NTSTATUS ntStatus = STATUS_SUCCESS; KSSTATE oldState = m_KsState; if (KsState == KSSTATE_RUN && oldState != KSSTATE_RUN) { m_ullPlayPosition = 0; m_ullWritePosition = 0; DPF(DPF_LEVEL_INFO, ("SetState: Transitioning to RUN. Data flow should begin.")); } else if (KsState == KSSTATE_STOP && oldState != KSSTATE_STOP) { DPF(DPF_LEVEL_INFO, ("SetState: Transitioning to STOP. Data flow should cease.")); } m_KsState = KsState; DPF_LEAVE(("CMiniportWaveRTLamaLoopbackStream::SetState, status=0x%08x", ntStatus)); return ntStatus; }

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::GetPosition - (Unchanged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::GetPosition(_Out_ PKSAUDIO_POSITION Position)
{ ASSERT(Position); if (!m_pPortStream || !m_pDataFormat || m_ulCurrentBufferSize == 0) { Position->PlayOffset = 0; Position->WriteOffset = 0; return (m_pPortStream && m_pDataFormat) ? STATUS_SUCCESS : STATUS_INVALID_DEVICE_STATE; } if (m_KsState == KSSTATE_STOP) { Position->PlayOffset = 0; Position->WriteOffset = 0; return STATUS_SUCCESS; } Position->PlayOffset = m_ullPlayPosition % m_ulCurrentBufferSize; Position->WriteOffset = m_ullWritePosition % m_ulCurrentBufferSize; return STATUS_SUCCESS; }

#pragma code_seg()
#pragma code_seg("NONPAGED")

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::AllocateAudioBuffer - (Unchanged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTLamaLoopbackStream::AllocateAudioBuffer(_In_ ULONG RequestedSize, _Out_ PMDL *AudioBufferMdl, _Out_ ULONG *ActualSize, _Out_ ULONG *OffsetFromFirstPage, _Out_ MEMORY_CACHING_TYPE *CacheType)
{ ASSERT(ActualSize); ASSERT(AudioBufferMdl); ASSERT(OffsetFromFirstPage); ASSERT(CacheType); if (!m_pPortStream || !m_pDataFormat) return STATUS_INVALID_DEVICE_STATE; NTSTATUS ntStatus = STATUS_SUCCESS; ULONG ulBlockAlign = m_pDataFormat->WaveFormatEx.nBlockAlign; if (ulBlockAlign == 0) ulBlockAlign = (m_ulChannelCount * m_ulBitsPerSample) / 8; if (ulBlockAlign == 0) ulBlockAlign = 1; ULONG ulAdjustedRequestedSize = RequestedSize; if (ulAdjustedRequestedSize % ulBlockAlign != 0) { ulAdjustedRequestedSize = ((ulAdjustedRequestedSize / ulBlockAlign) + 1) * ulBlockAlign; } if (ulAdjustedRequestedSize == 0) { ulAdjustedRequestedSize = (m_ulSampleRate * ulBlockAlign * LAMA_DEFAULT_PACKET_SIZE_MS) / 1000; if (ulAdjustedRequestedSize < ulBlockAlign) ulAdjustedRequestedSize = ulBlockAlign; if (ulAdjustedRequestedSize % ulBlockAlign != 0){ ulAdjustedRequestedSize = ((ulAdjustedRequestedSize / ulBlockAlign) + 1) * ulBlockAlign; } } if (ulAdjustedRequestedSize > m_ulMaxBufferSize) { ulAdjustedRequestedSize = m_ulMaxBufferSize; ulAdjustedRequestedSize -= (ulAdjustedRequestedSize % ulBlockAlign); } if (m_pAudioBufferMdl) { FreeAudioBuffer(m_pAudioBufferMdl, m_ulCurrentBufferSize); } ntStatus = m_pPortStream->AllocatePagesForMdl(ulAdjustedRequestedSize, AudioBufferMdl, ActualSize, OffsetFromFirstPage); if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("AllocateAudioBuffer: AllocatePagesForMdl failed 0x%x", ntStatus)); m_ulCurrentBufferSize = 0; m_pAudioBufferMdl = NULL; m_pAudioBuffer = NULL; return ntStatus; } m_pAudioBufferMdl = *AudioBufferMdl; m_ulCurrentBufferSize = *ActualSize; if (m_pAudioBufferMdl) { m_pAudioBuffer = MmGetSystemAddressForMdlSafe(m_pAudioBufferMdl, NormalPoolPriority); if (!m_pAudioBuffer) { DPF(DPF_LEVEL_ERROR, ("AllocateAudioBuffer: MmGetSystemAddressForMdlSafe failed.")); m_pPortStream->FreePagesFromMdl(m_pAudioBufferMdl); m_pAudioBufferMdl = NULL; m_ulCurrentBufferSize = 0; return STATUS_INSUFFICIENT_RESOURCES; } } else { m_pAudioBuffer = NULL; } *CacheType = MmCached; m_ullPlayPosition = 0; m_ullWritePosition = 0; DPF(DPF_LEVEL_INFO, ("AllocateAudioBuffer: Req=%u, AdjReq=%u, Actual=%u, VA=0x%p", RequestedSize, ulAdjustedRequestedSize, *ActualSize, m_pAudioBuffer)); return ntStatus; }

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::FreeAudioBuffer - (Unchanged)
//-----------------------------------------------------------------------------
STDMETHODIMP_(VOID)
CMiniportWaveRTLamaLoopbackStream::FreeAudioBuffer(_In_opt_ PMDL AudioBufferMdlParam, _In_ ULONG BufferSizeParam)
{ UNREFERENCED_PARAMETER(BufferSizeParam); m_pAudioBuffer = NULL; PMDL mdlToFree = AudioBufferMdlParam ? AudioBufferMdlParam : m_pAudioBufferMdl; if (mdlToFree) { if (m_pPortStream) { m_pPortStream->FreePagesFromMdl(mdlToFree); } if (mdlToFree == m_pAudioBufferMdl) { m_pAudioBufferMdl = NULL; } } m_ulCurrentBufferSize = 0; }

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::ProcessRenderDataFromWaveRtBuffer
// MODIFIED for Strategy A: Upmix client data to 16 channels in shared buffer
//-----------------------------------------------------------------------------
VOID CMiniportWaveRTLamaLoopbackStream::ProcessRenderDataFromWaveRtBuffer
(
    ULONG ulBufferOffset, // Offset in m_pAudioBuffer (client's WaveRT buffer)
    ULONG ulByteCount     // Byte count in client's format (e.g., stereo, 16-bit)
)
{
    KIRQL oldIrql;
    PLAMA_SHARED_LOOPBACK_BUFFER pCurrentBuffer = &g_InstanceLoopbackBuffers[m_instanceIndex];

    if (m_bCapture) return; // This is for render path
    if (!pCurrentBuffer->bInitialized || !pCurrentBuffer->pBuffer || !m_pAudioBuffer || ulByteCount == 0)
    {
        DPF(DPF_LEVEL_WARNING, ("ProcessRenderData (Inst %u): Not initialized or no data. SharedInit=%d, SharedBuf=0x%p, WaveRtBuf=0x%p, Count=%u",
            m_instanceIndex, pCurrentBuffer->bInitialized, pCurrentBuffer->pBuffer, m_pAudioBuffer, ulByteCount));
        return;
    }

    // Sanity checks for client format
    if (m_ulChannelCount == 0 || m_ulBitsPerSample == 0) {
        DPF(DPF_LEVEL_ERROR, ("ProcessRenderData (Inst %u): Client format not set (Ch=%u, Bits=%u). Aborting.", m_instanceIndex, m_ulChannelCount, m_ulBitsPerSample));
        return;
    }
    if (m_ulBitsPerSample != INTERNAL_DRIVER_BITS_PER_SAMPLE) {
         DPF(DPF_LEVEL_ERROR, ("ProcessRenderData (Inst %u): Client BitsPerSample (%u) does not match internal (%u). Aborting.",
            m_instanceIndex, m_ulBitsPerSample, INTERNAL_DRIVER_BITS_PER_SAMPLE));
        return;
    }

    PBYTE pSourceClientData = (PBYTE)m_pAudioBuffer + ulBufferOffset;
    USHORT clientSampleSize = (USHORT)(m_ulBitsPerSample / 8); // Bytes per sample
    ULONG clientFrameSize = m_ulChannelCount * clientSampleSize;
    ULONG numFrames = ulByteCount / clientFrameSize;

    if (numFrames == 0) {
        DPF(DPF_LEVEL_INFO, ("ProcessRenderData (Inst %u): Zero frames to process from ulByteCount %u.", m_instanceIndex, ulByteCount));
        return;
    }

    USHORT driverSampleSize = (USHORT)(INTERNAL_DRIVER_BITS_PER_SAMPLE / 8);
    ULONG driverFrameSize = INTERNAL_DRIVER_CHANNELS * driverSampleSize;
    ULONG totalDriverBytesToWrite = numFrames * driverFrameSize;

    KeAcquireSpinLock(&pCurrentBuffer->SpinLock, &oldIrql);

    ULONG currentWritePos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulWritePointer, 0, 0);
    ULONG currentReadPos = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulReadPointer, 0, 0);
    ULONG occupiedBytes = currentWritePos - currentReadPos;
    ULONG freeBytesInShared = pCurrentBuffer->ulBufferSize - occupiedBytes;

    if (totalDriverBytesToWrite > freeBytesInShared)
    {
        DPF(DPF_LEVEL_WARNING, ("ProcessRenderData (Inst %u): Shared buffer overflow. Need %u, Free: %u. Dropping %u frames.",
            m_instanceIndex, totalDriverBytesToWrite, freeBytesInShared, numFrames));
        KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);
        // Advance WaveRT buffer positions even if data is dropped
        m_ullPlayPosition = (m_ullPlayPosition + ulByteCount) % m_ulCurrentBufferSize;
        m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize;
        return;
    }

    PBYTE pClientFrame = pSourceClientData;
    for (ULONG i = 0; i < numFrames; ++i)
    {
        ULONG writeIdxShared = (currentWritePos + (i * driverFrameSize)) & (pCurrentBuffer->ulBufferSize - 1);
        PBYTE pDestDriverFrameStart = pCurrentBuffer->pBuffer + writeIdxShared;

        // Copy client channels
        ULONG clientBytesThisFrame = m_ulChannelCount * clientSampleSize;
        ULONG driverBytesToEndOfBuffer = pCurrentBuffer->ulBufferSize - writeIdxShared;

        if (driverFrameSize <= driverBytesToEndOfBuffer) // Whole driver frame fits without wrap
        {
            // Copy client data to start of driver frame
            RtlCopyMemory(pDestDriverFrameStart, pClientFrame, clientBytesThisFrame);
            // Zero out remaining channels in driver frame
            if (INTERNAL_DRIVER_CHANNELS > m_ulChannelCount) {
                RtlZeroMemory(pDestDriverFrameStart + clientBytesThisFrame,
                              (INTERNAL_DRIVER_CHANNELS - m_ulChannelCount) * driverSampleSize);
            }
        }
        else // Driver frame wraps around the shared buffer
        {
            PBYTE currentDestPtr = pDestDriverFrameStart;
            ULONG remainingDriverFrameBytes = driverFrameSize;

            // Part 1: Copy client channels, handling potential wrap for client data within driver frame part 1
            ULONG clientBytesToCopyPart1 = min(clientBytesThisFrame, driverBytesToEndOfBuffer);
            RtlCopyMemory(currentDestPtr, pClientFrame, clientBytesToCopyPart1);
            currentDestPtr += clientBytesToCopyPart1;
            remainingDriverFrameBytes -= clientBytesToCopyPart1;

            if (clientBytesToCopyPart1 < clientBytesThisFrame) { // Client data itself wrapped
                ULONG clientBytesRemaining = clientBytesThisFrame - clientBytesToCopyPart1;
                RtlCopyMemory(pCurrentBuffer->pBuffer, pClientFrame + clientBytesToCopyPart1, clientBytesRemaining);
                currentDestPtr = pCurrentBuffer->pBuffer + clientBytesRemaining;
                remainingDriverFrameBytes -= clientBytesRemaining;
            }

            // Part 2: Zero out remaining driver channels, handling wrap
            if (INTERNAL_DRIVER_CHANNELS > m_ulChannelCount) {
                ULONG silenceBytes = (INTERNAL_DRIVER_CHANNELS - m_ulChannelCount) * driverSampleSize;
                while (silenceBytes > 0) {
                    if (currentDestPtr >= pCurrentBuffer->pBuffer + pCurrentBuffer->ulBufferSize) { // ensure currentDestPtr wraps if needed
                        currentDestPtr = pCurrentBuffer->pBuffer;
                    }
                    ULONG bytesToZeroThisSegment = min(silenceBytes, (ULONG)(pCurrentBuffer->pBuffer + pCurrentBuffer->ulBufferSize - currentDestPtr));
                    RtlZeroMemory(currentDestPtr, bytesToZeroThisSegment);
                    currentDestPtr += bytesToZeroThisSegment;
                    silenceBytes -= bytesToZeroThisSegment;
                }
            }
        }
        pClientFrame += clientFrameSize; // Move to next client frame
    }

    InterlockedExchangeAdd((PLONG)&pCurrentBuffer->ulWritePointer, totalDriverBytesToWrite);
    KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);

    // Advance WaveRT buffer positions by client byte count
    m_ullPlayPosition = (m_ullPlayPosition + ulByteCount) % m_ulCurrentBufferSize;
    m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize;

    DPF(DPF_LEVEL_TRACE, ("ProcessRenderData (Inst %u, %u ch client): Copied %u frames (%u client bytes) as %u driver bytes to shared. New SharedWritePtr: %u",
        m_instanceIndex, m_ulChannelCount, numFrames, ulByteCount, totalDriverBytesToWrite, pCurrentBuffer->ulWritePointer));
}


//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::FetchCaptureDataToWaveRtBuffer
// (Largely Unchanged for Strategy A, but ensure it correctly handles reading
//  m_ulChannelCount from the 16-channel shared buffer)
//-----------------------------------------------------------------------------
VOID CMiniportWaveRTLamaLoopbackStream::FetchCaptureDataToWaveRtBuffer
(
    ULONG ulBufferOffset, // Offset in m_pAudioBuffer (client's WaveRT buffer)
    ULONG ulByteCount     // Byte count in client's format (e.g., stereo, 16-bit)
)
{
    KIRQL oldIrql;
    PLAMA_SHARED_LOOPBACK_BUFFER pCurrentBuffer = &g_InstanceLoopbackBuffers[m_instanceIndex];

    if (!m_bCapture) return; // This is for capture path
    if (!pCurrentBuffer->bInitialized || !pCurrentBuffer->pBuffer || !m_pAudioBuffer || ulByteCount == 0)
    {
         DPF(DPF_LEVEL_WARNING, ("FetchCaptureData (Inst %u): Not initialized or no data request. SharedInit=%d, SharedBuf=0x%p, WaveRtBuf=0x%p, Count=%u",
            m_instanceIndex, pCurrentBuffer->bInitialized, pCurrentBuffer->pBuffer, m_pAudioBuffer, ulByteCount));
        return;
    }

    // Sanity checks for client format
    if (m_ulChannelCount == 0 || m_ulBitsPerSample == 0) {
        DPF(DPF_LEVEL_ERROR, ("FetchCaptureData (Inst %u): Client format not set (Ch=%u, Bits=%u). Silencing.", m_instanceIndex, m_ulChannelCount, m_ulBitsPerSample));
        RtlZeroMemory((PBYTE)m_pAudioBuffer + ulBufferOffset, ulByteCount);
        // Advance WaveRT buffer positions
        m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize;
        m_ullPlayPosition = m_ullWritePosition;
        return;
    }
    if (m_ulBitsPerSample != INTERNAL_DRIVER_BITS_PER_SAMPLE) {
         DPF(DPF_LEVEL_ERROR, ("FetchCaptureData (Inst %u): Client BitsPerSample (%u) does not match internal (%u). Silencing.",
            m_instanceIndex, m_ulBitsPerSample, INTERNAL_DRIVER_BITS_PER_SAMPLE));
        RtlZeroMemory((PBYTE)m_pAudioBuffer + ulBufferOffset, ulByteCount);
        m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize;
        m_ullPlayPosition = m_ullWritePosition;
        return;
    }

    PBYTE pDestClientData = (PBYTE)m_pAudioBuffer + ulBufferOffset;
    USHORT clientSampleSize = (USHORT)(m_ulBitsPerSample / 8);
    ULONG clientFrameSize = m_ulChannelCount * clientSampleSize;
    ULONG numFramesToClient = ulByteCount / clientFrameSize;

    USHORT driverSampleSize = (USHORT)(INTERNAL_DRIVER_BITS_PER_SAMPLE / 8);
    ULONG driverFrameSize = INTERNAL_DRIVER_CHANNELS * driverSampleSize;

    ULONG totalBytesReadFromSharedActual = 0; // Actual client bytes copied

    KeAcquireSpinLock(&pCurrentBuffer->SpinLock, &oldIrql);

    ULONG currentWritePosShared = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulWritePointer, 0, 0);
    ULONG currentReadPosShared = InterlockedCompareExchange((PLONG)&pCurrentBuffer->ulReadPointer, 0, 0);
    ULONG availableDriverBytesInShared = currentWritePosShared - currentReadPosShared;
    ULONG availableDriverFramesInShared = availableDriverBytesInShared / driverFrameSize;

    ULONG framesToCopy = min(numFramesToClient, availableDriverFramesInShared);
    ULONG bytesAdvancedInShared = 0;

    if (framesToCopy > 0)
    {
        PBYTE pClientFrameDest = pDestClientData;
        for (ULONG i = 0; i < framesToCopy; ++i)
        {
            ULONG readIdxShared = (currentReadPosShared + bytesAdvancedInShared) & (pCurrentBuffer->ulBufferSize - 1);
            PBYTE pSourceDriverFrameStart = pCurrentBuffer->pBuffer + readIdxShared;
            ULONG driverBytesToEndOfBuffer = pCurrentBuffer->ulBufferSize - readIdxShared;

            if (driverFrameSize <= driverBytesToEndOfBuffer) // Whole driver frame is contiguous
            {
                RtlCopyMemory(pClientFrameDest, pSourceDriverFrameStart, clientFrameSize); // Copy only client's channel portion
            }
            else // Driver frame wraps in shared buffer
            {
                ULONG firstPartLen = driverBytesToEndOfBuffer;
                if (clientFrameSize <= firstPartLen) { // Client part fits in first segment
                    RtlCopyMemory(pClientFrameDest, pSourceDriverFrameStart, clientFrameSize);
                } else { // Client part also wraps
                    RtlCopyMemory(pClientFrameDest, pSourceDriverFrameStart, firstPartLen);
                    RtlCopyMemory(pClientFrameDest + firstPartLen, pCurrentBuffer->pBuffer, clientFrameSize - firstPartLen);
                }
            }
            pClientFrameDest += clientFrameSize;
            bytesAdvancedInShared += driverFrameSize; // Advance by a full driver frame in shared buffer
        }
        InterlockedExchangeAdd((PLONG)&pCurrentBuffer->ulReadPointer, bytesAdvancedInShared);
        totalBytesReadFromSharedActual = framesToCopy * clientFrameSize;
    }

    KeReleaseSpinLock(&pCurrentBuffer->SpinLock, oldIrql);

    if (totalBytesReadFromSharedActual < ulByteCount)
    {
        // Silence the remaining part of the client's buffer
        RtlZeroMemory(pDestClientData + totalBytesReadFromSharedActual, ulByteCount - totalBytesReadFromSharedActual);
        DPF(DPF_LEVEL_INFO, ("FetchCaptureData (Inst %u): Shared buffer underrun. Copied %u client frames (%u bytes), Silenced %u bytes.",
            m_instanceIndex, framesToCopy, totalBytesReadFromSharedActual, ulByteCount - totalBytesReadFromSharedActual));
    }

    // Advance WaveRT buffer positions
    m_ullWritePosition = (m_ullWritePosition + ulByteCount) % m_ulCurrentBufferSize;
    m_ullPlayPosition = m_ullWritePosition;

    DPF(DPF_LEVEL_TRACE, ("FetchCaptureData (Inst %u, %u ch client): Copied %u client frames. New SharedReadPtr: %u",
        m_instanceIndex, m_ulChannelCount, framesToCopy, pCurrentBuffer->ulReadPointer));
}


//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackStream::NotificationDpcRoutine - (Unchanged)
//-----------------------------------------------------------------------------
_Use_decl_annotations_
VOID CMiniportWaveRTLamaLoopbackStream::NotificationDpcRoutine
( PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{ UNREFERENCED_PARAMETER(Dpc); UNREFERENCED_PARAMETER(SystemArgument1); UNREFERENCED_PARAMETER(SystemArgument2); CMiniportWaveRTLamaLoopbackStream *pStream = (CMiniportWaveRTLamaLoopbackStream*)DeferredContext; if (!pStream) return; if (pStream->m_KsState != KSSTATE_RUN) return; ULONG bufferSize = pStream->m_ulCurrentBufferSize; if (bufferSize == 0) return; ULONG notificationInterval = bufferSize / 2; if (!pStream->m_bCapture) { ULONG currentWaveRtPlayOffset = pStream->m_ullPlayPosition % bufferSize; pStream->ProcessRenderDataFromWaveRtBuffer(currentWaveRtPlayOffset, notificationInterval); } else { ULONG currentWaveRtWriteOffset = pStream->m_ullWritePosition % bufferSize; pStream->FetchCaptureDataToWaveRtBuffer(currentWaveRtWriteOffset, notificationInterval); } }

#pragma code_seg("PAGE")
//-----------------------------------------------------------------------------
// Other IMiniportWaveRTStream methods - (Unchanged)
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
// (Unchanged - This path is used by the plugin which already provides 16ch data)
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
// (Unchanged - This path is used by the plugin which expects 16ch data)
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