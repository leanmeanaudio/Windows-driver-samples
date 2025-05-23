#include <ntddk.h>
#include <wdm.h>
#include <portcls.h>
#include "lamaloopbackcommon.h" // For globals, property GUIDs, common formats, etc.
#include "lamaloopbackrender.h"
#include "lamaloopbackstream.h" 
#include "baseaddress.h" 
#include "resource.h" 

// If LAMA_POOL_TAG is not defined elsewhere (it should be in common.h or resource.h)
#ifndef LAMA_POOL_TAG
#define LAMA_POOL_TAG 'RLaL' // Lama Loopback Render
#endif

#pragma code_seg("PAGE")

//=============================================================================
// KSPROPERTY_LAMA_SAMPLE_RATE Handler
//=============================================================================
NTSTATUS
LamaFilterPropertyHandler_SampleRate
(
    _In_ PIRP Irp,
    _In_ PKSPROPERTY Property,
    _Inout_ PVOID Data
)
{
    PAGED_CODE();
    ASSERT(Irp);
    ASSERT(Property);
    ASSERT(Data);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    DPF_ENTER(("[LamaFilterPropertyHandler_SampleRate] PropertyId=%u, Flags=0x%X", Property->Id, Property->Flags));

    if (Property->Id != KSPROPERTY_LAMA_SAMPLE_RATE)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Property->Flags & KSPROPERTY_TYPE_GET)
    {
        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG))
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
        }
        else
        {
            *(PULONG)Data = g_CurrentGlobalSampleRate;
            ntStatus = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(ULONG);
        }
    }
    else if (Property->Flags & KSPROPERTY_TYPE_SET)
    {
        if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG))
        {
            ntStatus = STATUS_INVALID_PARAMETER; // Or STATUS_BUFFER_TOO_SMALL
            Irp->IoStatus.Information = 0;
        }
        else
        {
            ULONG newSampleRate = *(PULONG)Data;
            // Validate against the device's capabilities (MIN_SAMPLE_RATE_PCM, MAX_SAMPLE_RATE_PCM from common.h)
            if (newSampleRate >= MIN_SAMPLE_RATE_PCM && newSampleRate <= MAX_SAMPLE_RATE_PCM)
            {
                // This is a simplified check. A real driver might need to check if the rate
                // is one of a few specifically supported rates, not just within a range.
                g_CurrentGlobalSampleRate = newSampleRate;
                // Potentially re-evaluate active streams or require streams to be recreated.
                // For now, just update the global.
                DPF(DPF_LEVEL_INFO, ("Set new global sample rate: %u Hz", g_CurrentGlobalSampleRate));
                ntStatus = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(ULONG);
            }
            else
            {
                DPF(DPF_LEVEL_ERROR, ("Invalid sample rate set: %u Hz. Valid range %u-%u Hz.", 
                    newSampleRate, MIN_SAMPLE_RATE_PCM, MAX_SAMPLE_RATE_PCM));
                ntStatus = STATUS_INVALID_PARAMETER;
                Irp->IoStatus.Information = 0;
            }
        }
    }
    else if (Property->Flags & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        PKSPROPERTY_DESCRIPTION PropDesc = (PKSPROPERTY_DESCRIPTION)Data;
        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(KSPROPERTY_DESCRIPTION))
        {
             ntStatus = STATUS_BUFFER_TOO_SMALL;
             Irp->IoStatus.Information = 0;
        }
        else
        {
            PropDesc->AccessFlags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT;
            PropDesc->DescriptionSize = sizeof(KSPROPERTY_DESCRIPTION);
            PropDesc->PropTypeSet.Set = KSPROPSETID_LamaLoopback; 
            PropDesc->PropTypeSet.Id = KSPROPERTY_LAMA_SAMPLE_RATE;
            PropDesc->PropTypeSet.Flags = 0; // Not used for this
            PropDesc->MembersListCount = 0;
            PropDesc->Reserved = 0;
            ntStatus = STATUS_SUCCESS;
            Irp->IoStatus.Information = sizeof(KSPROPERTY_DESCRIPTION);
        }
    }
    else
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
    }
    
    Irp->IoStatus.Status = ntStatus;
    // IoCompleteRequest(Irp, IO_NO_INCREMENT); // Not needed if PcHandleProperty is used by PortCls
    DPF_LEAVE(("[LamaFilterPropertyHandler_SampleRate] ntStatus=0x%08x", ntStatus));
    return ntStatus;
}


//=============================================================================
// Automation Table for KSPROPSETID_LamaLoopback
//=============================================================================
DEFINE_KSPROPERTY_TABLE(LamaLoopbackFilterPropertyTable)
{
    DEFINE_KSPROPERTY_ITEM_LAMA_SAMPLE_RATE(
        LamaFilterPropertyHandler_SampleRate, // Get Handler
        LamaFilterPropertyHandler_SampleRate  // Set Handler
        // Basic support is implicitly handled by the handler if KSPROPERTY_TYPE_BASICSUPPORT is checked
    )
};

DEFINE_KSPROPERTY_SET_TABLE(LamaLoopbackFilterAutomationTable)
{
    DEFINE_KSPROPERTY_SET(
        &KSPROPSETID_LamaLoopback,                          // Set GUID
        SIZEOF_ARRAY(LamaLoopbackFilterPropertyTable),      // PropertiesCount
        LamaLoopbackFilterPropertyTable,                    // PropertyItem
        0,                                                  // FastIoCount
        NULL                                                // FastIoTable
    )
};


//=============================================================================
// CMiniportTopologyLamaLoopbackRender
//=============================================================================
CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender(void)
:   CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender]")); }

CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender(void)
{
    PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender]"));
    if (m_Port) { m_Port->Release(); m_Port = NULL; }
    if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; }
}

NTSTATUS CMiniportTopologyLamaLoopbackRender::Init
( _In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY Port_ )
{
    PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::Init]"));
    UNREFERENCED_PARAMETER(ResourceList);
    m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef();
    m_Port = Port_; m_Port->AddRef();
    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackRender::DataRangeIntersection
//
// Handles data range intersection queries.
// Updated to use global sample rate, channels, and bit depth.
//-----------------------------------------------------------------------------
NTSTATUS
CMiniportTopologyLamaLoopbackRender::DataRangeIntersection
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
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::DataRangeIntersection] PinId=%u", PinId));

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
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection: No match due to Major/Sub/Specifier GUIDs."));
        return STATUS_NO_MATCH;
    }

    PKSDATARANGE_AUDIO clientDataRangeAudio = (PKSDATARANGE_AUDIO)DataRange;

    // Check against our device's capabilities (defined in PcmAudioDataRange from common.h)
    // For this example, we are somewhat flexible on channels/bits based on client request
    // but sample rate is fixed by g_CurrentGlobalSampleRate.
    // A more robust check would compare clientDataRangeAudio against PcmAudioDataRange fields.
    
    ULONG resultChannels = g_CurrentGlobalChannels; // Use global default
    ULONG resultBitsPerSample = g_CurrentGlobalBitsPerSample; // Use global default
    ULONG resultSampleRate = g_CurrentGlobalSampleRate; // CRITICAL: Use global sample rate

    // Attempt to select a format based on intersection.
    // For simplicity, if the client asks for a range, we see if our global settings fit.
    // A more complex driver might pick the "best" fit from the client's range that matches driver caps.

    // If clientDataRangeAudio specifies specific values (Min == Max), check them.
    if (clientDataRangeAudio->MaximumChannels != 0 && // 0 can mean any
        clientDataRangeAudio->MaximumChannels < resultChannels) {
        resultChannels = clientDataRangeAudio->MaximumChannels; // Or reject if too low.
    }
     if (clientDataRangeAudio->MinimumChannels > resultChannels) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection: No match on min channels. ClientMin=%u, GlobalCurrent=%u", 
            clientDataRangeAudio->MinimumChannels, resultChannels));
        return STATUS_NO_MATCH;
    }
    // Similar checks for BitsPerSample if they were also global/configurable.
    // For SampleRate, we enforce g_CurrentGlobalSampleRate.
    // Check if g_CurrentGlobalSampleRate is within the client's requested range.
    if (resultSampleRate < clientDataRangeAudio->MinimumSampleRate ||
        resultSampleRate > clientDataRangeAudio->MaximumSampleRate)
    {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection: Global SR %u not in client range [%u, %u]",
            resultSampleRate, clientDataRangeAudio->MinimumSampleRate, clientDataRangeAudio->MaximumSampleRate));
        return STATUS_NO_MATCH; 
    }

    // Ensure chosen format is within device's absolute min/max capabilities
    if (resultChannels < MIN_CHANNELS_PCM || resultChannels > MAX_CHANNELS_PCM ||
        resultBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || resultBitsPerSample > MAX_BITS_PER_SAMPLE_PCM ||
        resultSampleRate < MIN_SAMPLE_RATE_PCM || resultSampleRate > MAX_SAMPLE_RATE_PCM) {
        DPF(DPF_LEVEL_INFO, ("DataRangeIntersection: Resulting format (%uCh, %ubit, %uHz) outside device capabilities.",
            resultChannels, resultBitsPerSample, resultSampleRate));
        return STATUS_NO_MATCH;
    }


    if (!ResultantFormat) // If only querying for size
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
        return STATUS_BUFFER_OVERFLOW; // Standard way to indicate size needed
    }

    if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))
    {
        DPF(DPF_LEVEL_ERROR, ("DataRangeIntersection: Output buffer too small. Needed %u, Got %u", 
            (ULONG)sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), OutputBufferLength));
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Construct the KSDATAFORMAT_WAVEFORMATEXTENSIBLE
    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pResFormatWfx = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat;
    
    pResFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pResFormatWfx->DataFormat.Flags = 0;
    pResFormatWfx->DataFormat.SampleSize = 0; // Optional, usually derived from format
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
    
    pWfx->Samples.wValidBitsPerSample = pWfx->Format.wBitsPerSample; // Or actual valid bits if different
    
    if (pWfx->Format.nChannels == 1) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_MONO;
    } else if (pWfx->Format.nChannels == 2) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    } else {
        pWfx->dwChannelMask = 0; // Or a more complex mask if applicable for > stereo
    }
    pWfx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    DPF(DPF_LEVEL_INFO, ("DataRangeIntersection: Success. Format: %uHz, %uch, %ubit", 
        pWfx->Format.nSamplesPerSec, pWfx->Format.nChannels, pWfx->Format.wBitsPerSample));
    
    return STATUS_SUCCESS;
}


NTSTATUS CMiniportTopologyLamaLoopbackRender::NonDelegatingQueryInterface
( _In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    PAGED_CODE(); ASSERT(Object);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::NonDelegatingQueryInterface]"));
    if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport))
    { *Object = PVOID(PUNKNOWN(this)); }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
    { *Object = PVOID(PMINIPORTTOPOLOGY(this)); }
    else { *Object = NULL; return STATUS_NOT_SUPPORTED; }
    ((PUNKNOWN)*Object)->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS CreateMiniportTopologyLamaLoopbackRender
( _Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{
    PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportTopologyLamaLoopbackRender]"));
    UNREFERENCED_PARAMETER(UnknownOuter);
    CMiniportTopologyLamaLoopbackRender *obj = new (PoolType, LAMA_POOL_TAG) CMiniportTopologyLamaLoopbackRender;
    if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; }
    *Unknown = PUNKNOWN((PMINIPORTTOPOLOGY)obj); (*Unknown)->AddRef();
    return STATUS_SUCCESS;
}

//=============================================================================
// CMiniportWaveRTLamaLoopbackRender
//=============================================================================
CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender(void)
:   CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender]")); }

CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender(void)
{
    PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender]"));
    if (m_Port) { m_Port->Release(); m_Port = NULL; }
    if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; }
}

NTSTATUS CMiniportWaveRTLamaLoopbackRender::Init
( _In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port_ )
{
    PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::Init]"));
    UNREFERENCED_PARAMETER(ResourceList);
    m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef();
    m_Port = Port_; m_Port->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS CMiniportWaveRTLamaLoopbackRender::NewStream
( _Out_ PMINIPORTWAVERTSTREAM * Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat)
{
    PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::NewStream]"));
    ASSERT (Stream); ASSERT (DataFormat); ASSERT (PortStream);
    UNREFERENCED_PARAMETER(Pin); 
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PCMiniportWaveRTLamaLoopbackStream newStream = NULL;
    *Stream = NULL;
    newStream = new (NonPagedPoolNx, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackStream(NULL); 
    if (newStream == NULL) {
        DPF(DPF_LEVEL_ERROR, ("Failed to allocate CMiniportWaveRTLamaLoopbackStream"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES; goto Done;
    }
    ntStatus = newStream->Init(PortStream, DataFormat, Capture); 
    if (!NT_SUCCESS(ntStatus)) {
        DPF(DPF_LEVEL_ERROR, ("Failed to initialize CMiniportWaveRTLamaLoopbackStream: 0x%x", ntStatus));
        goto Done;
    }
    ntStatus = newStream->QueryInterface(IID_IMiniportWaveRTStream, (PVOID*)Stream);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(DPF_LEVEL_ERROR, ("QueryInterface for IMiniportWaveRTStream failed: 0x%x", ntStatus));
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

NTSTATUS CMiniportWaveRTLamaLoopbackRender::NonDelegatingQueryInterface
( _In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{
    PAGED_CODE(); ASSERT(Object);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::NonDelegatingQueryInterface]"));
    if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport))
    { *Object = PVOID(PUNKNOWN(this)); }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT))
    { *Object = PVOID(PMINIPORTWAVERT(this)); }
    else { *Object = NULL; return STATUS_NOT_SUPPORTED; }
    ((PUNKNOWN)*Object)->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS CreateMiniportWaveRTLamaLoopbackRender
( _Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{
    PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportWaveRTLamaLoopbackRender]"));
    UNREFERENCED_PARAMETER(UnknownOuter);
    CMiniportWaveRTLamaLoopbackRender *obj = new (PoolType, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackRender;
    if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; }
    *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj); (*Unknown)->AddRef();
    return STATUS_SUCCESS;
}

#pragma code_seg() // End PAGED_CODE segment
