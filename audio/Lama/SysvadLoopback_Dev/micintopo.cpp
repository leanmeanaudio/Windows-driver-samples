#include <ntddk.h>
#include <wdm.h>
#include <portcls.h>
#include "lamaloopbackcommon.h" // For globals, property GUIDs, common formats, etc.
#include "lamaloopbackcapture.h"
#include "lamaloopbackstream.h" 
#include "baseaddress.h" 
#include "resource.h" 

// If LAMA_POOL_TAG is not defined elsewhere (it should be in common.h or resource.h)
#ifndef LAMA_POOL_TAG
#define LAMA_POOL_TAG 'CLaL' // Lama Loopback Capture
#endif

#pragma code_seg("PAGE")
//=============================================================================
// CMiniportTopologyLamaLoopbackCapture
//=============================================================================
CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture(void)
:   CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture]")); }

CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture(void)
{
    PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture]"));
    if (m_Port) { m_Port->Release(); m_Port = NULL; }
    if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; }
}

NTSTATUS CMiniportTopologyLamaLoopbackCapture::Init
( _In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY Port_ )
{
    PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::Init]"));
    UNREFERENCED_PARAMETER(ResourceList);
    m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef();
    m_Port = Port_; m_Port->AddRef();
    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection
//
// Handles data range intersection queries.
// Updated to use global sample rate, channels, and bit depth.
//-----------------------------------------------------------------------------
NTSTATUS
CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection
(
    _In_        ULONG           PinId,
    _In_        PKSDATARANGE    DataRange,          // Client's proposed data range
    _In_        PKSDATARANGE    MatchingDataRange,  // Pin's data range (PcmAudioDataRange)
    _In_        ULONG           OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID           ResultantFormat,    // Buffer for the resulting format
    _Out_       PULONG          ResultantFormatLength
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection] PinId=%u", PinId));

    UNREFERENCED_PARAMETER(MatchingDataRange); // We use our PcmAudioDataRange internally

    if (!DataRange || !ResultantFormatLength) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *ResultantFormatLength = 0; // Default to zero

    // Validate basic format type (Audio, PCM, WaveFormatEx)
    if (!IsEqualGUIDAligned(DataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(DataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(DataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): No match due to Major/Sub/Specifier GUIDs."));
        return STATUS_NO_MATCH;
    }

    PKSDATARANGE_AUDIO clientDataRangeAudio = (PKSDATARANGE_AUDIO)DataRange;
    
    ULONG clientMinChannels = clientDataRangeAudio->MinimumChannels;
    ULONG clientMaxChannels = clientDataRangeAudio->MaximumChannels;
    ULONG resultChannels;

    if (clientMaxChannels == 0 || clientMaxChannels > MAX_CHANNELS_PCM) {
        clientMaxChannels = MAX_CHANNELS_PCM;
    }
   
    if (clientMinChannels > clientMaxChannels) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Client min channels %u > client max channels %u. No match.", clientMinChannels, clientMaxChannels));
        return STATUS_NO_MATCH; 
    }
    
    if (clientMaxChannels >= MIN_CHANNELS_PCM && clientMaxChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMaxChannels;
    } else if (clientMinChannels >= MIN_CHANNELS_PCM && clientMinChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMinChannels;
    } else {
         DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Client channel range [%u, %u] is outside device capabilities [%u, %u]. No match.",
             clientMinChannels, clientMaxChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM));
        return STATUS_NO_MATCH;
    }
   
    if (resultChannels < MIN_CHANNELS_PCM || resultChannels > MAX_CHANNELS_PCM) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Selected resultChannels %u is outside device capabilities [%u, %u]. No match.",
            resultChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM));
        return STATUS_NO_MATCH;
    }

    // Sample Rate and Bits Per Sample logic
    ULONG resultBitsPerSample = g_CurrentGlobalBitsPerSample; 
    ULONG resultSampleRate = g_CurrentGlobalSampleRate;      

    if (resultBitsPerSample < clientDataRangeAudio->MinimumBitsPerSample ||
        resultBitsPerSample > clientDataRangeAudio->MaximumBitsPerSample) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Global BPS %u not in client range [%u, %u]",
            resultBitsPerSample, clientDataRangeAudio->MinimumBitsPerSample, clientDataRangeAudio->MaximumBitsPerSample));
        return STATUS_NO_MATCH;
    }

    if (resultSampleRate < clientDataRangeAudio->MinimumSampleRate ||
        resultSampleRate > clientDataRangeAudio->MaximumSampleRate) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Global SR %u not in client range [%u, %u]",
            resultSampleRate, clientDataRangeAudio->MinimumSampleRate, clientDataRangeAudio->MaximumSampleRate));
        return STATUS_NO_MATCH;
    }

    if (resultBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || resultBitsPerSample > MAX_BITS_PER_SAMPLE_PCM ||
        resultSampleRate < MIN_SAMPLE_RATE_PCM || resultSampleRate > MAX_SAMPLE_RATE_PCM) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Resulting format SR/BPS (%uHz, %ubit) outside device capabilities.",
            resultSampleRate, resultBitsPerSample));
        return STATUS_NO_MATCH;
    }


    if (!ResultantFormat) 
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
        return STATUS_BUFFER_OVERFLOW; 
    }

    if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))
    {
        DPF(DPF_LEVEL_ERROR, ("DataRangeIntersection (Capture): Output buffer too small. Needed %u, Got %u", 
            (ULONG)sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), OutputBufferLength));
        return STATUS_BUFFER_TOO_SMALL;
    }

    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pResFormatWfx = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat;
    
    pResFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pResFormatWfx->DataFormat.Flags = 0;
    pResFormatWfx->DataFormat.SampleSize = (resultBitsPerSample / 8) * resultChannels; 
    pResFormatWfx->DataFormat.Reserved = 0;
    pResFormatWfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    pResFormatWfx->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    pResFormatWfx->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    WAVEFORMATEXTENSIBLE *pWfx = &pResFormatWfx->WaveFormatExt;
    pWfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    pWfx->Format.nChannels = (WORD)resultChannels;
    pWfx->Format.nSamplesPerSec = resultSampleRate;
    pWfx->Format.wBitsPerSample = (WORD)resultBitsPerSample;
    pWfx->Format.nBlockAlign = (WORD)((pWfx->Format.nChannels * pWfx->Format.wBitsPerSample) / 8);
    pWfx->Format.nAvgBytesPerSec = pWfx->Format.nSamplesPerSec * pWfx->Format.nBlockAlign;
    pWfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    
    pWfx->Samples.wValidBitsPerSample = pWfx->Format.wBitsPerSample;
    
    if (pWfx->Format.nChannels == 1) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_MONO;
    } else if (pWfx->Format.nChannels == 2) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    } else {
        pWfx->dwChannelMask = 0; 
    }
    pWfx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Success. Format: %uHz, %uch, %ubit", 
        pWfx->Format.nSamplesPerSec, pWfx->Format.nChannels, pWfx->Format.wBitsPerSample));
    
    return STATUS_SUCCESS;
}


NTSTATUS CMiniportTopologyLamaLoopbackCapture::NonDelegatingQueryInterface
( _In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    PAGED_CODE(); ASSERT(Object);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::NonDelegatingQueryInterface]"));
    if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport))
    { *Object = PVOID(PUNKNOWN(this)); }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
    { *Object = PVOID(PMINIPORTTOPOLOGY(this)); }
    else { *Object = NULL; return STATUS_NOT_SUPPORTED; }
    ((PUNKNOWN)*Object)->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS CreateMiniportTopologyLamaLoopbackCapture
( _Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{
    PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportTopologyLamaLoopbackCapture]"));
    UNREFERENCED_PARAMETER(UnknownOuter);
    CMiniportTopologyLamaLoopbackCapture *obj = new (PoolType, LAMA_POOL_TAG) CMiniportTopologyLamaLoopbackCapture;
    if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; }
    *Unknown = PUNKNOWN((PMINIPORTTOPOLOGY)obj); (*Unknown)->AddRef();
    return STATUS_SUCCESS;
}

//=============================================================================
// CMiniportWaveRTLamaLoopbackCapture
//=============================================================================
CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture(void)
:   CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture]")); }

CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture(void)
{
    PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture]"));
    if (m_Port) { m_Port->Release(); m_Port = NULL; }
    if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; }
}

NTSTATUS CMiniportWaveRTLamaLoopbackCapture::Init
( _In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port_ )
{
    PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::Init]"));
    UNREFERENCED_PARAMETER(ResourceList);
    m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef();
    m_Port = Port_; m_Port->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS CMiniportWaveRTLamaLoopbackCapture::NewStream
( _Out_ PMINIPORTWAVERTSTREAM * Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat)
{
    PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::NewStream]"));
    ASSERT (Stream); ASSERT (DataFormat); ASSERT (PortStream);
    UNREFERENCED_PARAMETER(Pin); 
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PCMiniportWaveRTLamaLoopbackStream newStream = NULL;
    *Stream = NULL;
    newStream = new (NonPagedPoolNx, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackStream(NULL); 
    if (newStream == NULL) {
        DPF(DPF_LEVEL_ERROR, ("Failed to allocate CMiniportWaveRTLamaLoopbackStream for capture"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES; goto Done;
    }
    ntStatus = newStream->Init(PortStream, DataFormat, Capture); 
    if (!NT_SUCCESS(ntStatus)) {
        DPF(DPF_LEVEL_ERROR, ("Failed to initialize CMiniportWaveRTLamaLoopbackStream for capture: 0x%x", ntStatus));
        goto Done;
    }
    ntStatus = newStream->QueryInterface(IID_IMiniportWaveRTStream, (PVOID*)Stream);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(DPF_LEVEL_ERROR, ("QueryInterface for IMiniportWaveRTStream failed for capture: 0x%x", ntStatus));
        goto Done;
    }
Done:
    if (!NT_SUCCESS(ntStatus)) {
        if (newStream) { newStream->Release(); }
        if (Stream != NULL) { *Stream = NULL; }
    } else {
        if (newStream) { newStream->Release(); }
    }
    return ntStatus;
}

NTSTATUS CMiniportWaveRTLamaLoopbackCapture::NonDelegatingQueryInterface
( _In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    PAGED_CODE(); ASSERT(Object);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::NonDelegatingQueryInterface]"));
    if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport))
    { *Object = PVOID(PUNKNOWN(this)); }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT))
    { *Object = PVOID(PMINIPORTWAVERT(this)); }
    else { *Object = NULL; return STATUS_NOT_SUPPORTED; }
    ((PUNKNOWN)*Object)->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS CreateMiniportWaveRTLamaLoopbackCapture
( _Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{
    PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportWaveRTLamaLoopbackCapture]"));
    UNREFERENCED_PARAMETER(UnknownOuter);
    CMiniportWaveRTLamaLoopbackCapture *obj = new (PoolType, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackCapture;
    if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; }
    *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj); (*Unknown)->AddRef();
    return STATUS_SUCCESS;
}

#pragma code_seg() // End PAGED_CODE segment
