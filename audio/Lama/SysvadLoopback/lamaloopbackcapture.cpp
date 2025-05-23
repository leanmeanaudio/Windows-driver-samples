#include <ntddk.h>
#include <wdm.h>
#include <portcls.h>
#include "lamaloopbackcapture.h"
#include "baseaddress.h" // For DPF_ENTER etc.

#pragma code_seg("PAGE")
//=============================================================================
// CMiniportTopologyLamaLoopbackCapture
//=============================================================================

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture
//
// Constructor for the topology miniport.
//
CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture
(
    void
)
:   CUnknown(NULL),
    m_Port(NULL),
    m_UnknownAdapter(NULL)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture]"));
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture
//
// Destructor for the topology miniport.
//
CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture
(
    void
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture]"));

    if (m_Port)
    {
        m_Port->Release();
        m_Port = NULL;
    }
    if (m_UnknownAdapter)
    {
        m_UnknownAdapter->Release();
        m_UnknownAdapter = NULL;
    }
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackCapture::Init
//
// Initializes the topology miniport.
//
NTSTATUS
CMiniportTopologyLamaLoopbackCapture::Init
(
    _In_  PUNKNOWN        UnknownAdapter,
    _In_  PRESOURCELIST   ResourceList,
    _In_  PPORTTOPOLOGY   Port_
)
{
    PAGED_CODE();
    ASSERT(UnknownAdapter);
    ASSERT(Port_);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::Init]"));

    UNREFERENCED_PARAMETER(ResourceList);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_UnknownAdapter = UnknownAdapter;
    m_UnknownAdapter->AddRef();

    m_Port = Port_;
    m_Port->AddRef();

    // The filter descriptor is specified in lamaloopbackcapture.h
    // No further initialization of pins/nodes needed here for passthrough.

    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection
//
// Handles data range intersection queries.
// This is a simplified version. A real driver would need to check PinId
// and offer appropriate formats.
//
NTSTATUS
CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection
(
    _In_        ULONG           PinId,
    _In_        PKSDATARANGE    DataRange,
    _In_        PKSDATARANGE    MatchingDataRange,
    _In_        ULONG           OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID           ResultantFormat,
    _Out_       PULONG          ResultantFormatLength
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection]"));

    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(DataRange);
    UNREFERENCED_PARAMETER(MatchingDataRange);

    if (!ResultantFormat)
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
        return STATUS_BUFFER_OVERFLOW;
    }

    if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))
    {
        *ResultantFormatLength = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Default to Pcm48000_16ch_16bit
    RtlCopyMemory(ResultantFormat, &Pcm48000_16ch_16bit, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    
    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackCapture NonDelegatingQueryInterface
//
// Obtains an interface.
//
NTSTATUS
CMiniportTopologyLamaLoopbackCapture::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * Object
)
{
    PAGED_CODE();
    ASSERT(Object);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::NonDelegatingQueryInterface]"));

    if (IsEqualGUIDAligned(Interface, IID_IUnknown) ||
        IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
    {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
    }
    else
    {
        *Object = NULL;
        return STATUS_NOT_SUPPORTED; // Corrected
    }

    PUNKNOWN(*Object)->AddRef();
    return STATUS_SUCCESS;
}

//=============================================================================
// Factory function for Capture Topology Miniport
//=============================================================================
NTSTATUS
CreateMiniportTopologyLamaLoopbackCapture
(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
)
{
    PAGED_CODE();
    ASSERT(Unknown);
    DPF_ENTER(("[CreateMiniportTopologyLamaLoopbackCapture]"));

    UNREFERENCED_PARAMETER(UnknownOuter);

    CMiniportTopologyLamaLoopbackCapture *obj = new (PoolType, MINIPORT_POOLTAG) CMiniportTopologyLamaLoopbackCapture;
    if (NULL == obj)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    *Unknown = PUNKNOWN((PMINIPORTTOPOLOGY)obj);
    (*Unknown)->AddRef(); // The caller expects a referenced object

    return STATUS_SUCCESS;
}


//=============================================================================
// CMiniportWaveRTLamaLoopbackCapture
//=============================================================================

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture
//
// Constructor for the wave miniport.
//
CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture
(
    void
)
:   CUnknown(NULL),
    m_Port(NULL),
    m_UnknownAdapter(NULL)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture]"));
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture
//
// Destructor for the wave miniport.
//
CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture
(
    void
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture]"));

    if (m_Port)
    {
        m_Port->Release();
        m_Port = NULL;
    }
    if (m_UnknownAdapter)
    {
        m_UnknownAdapter->Release();
        m_UnknownAdapter = NULL;
    }
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackCapture::Init
//
// Initializes the wave miniport.
//
NTSTATUS
CMiniportWaveRTLamaLoopbackCapture::Init
(
    _In_  PUNKNOWN        UnknownAdapter,
    _In_  PRESOURCELIST   ResourceList,
    _In_  PPORTWAVERT     Port_
)
{
    PAGED_CODE();
    ASSERT(UnknownAdapter);
    ASSERT(Port_);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::Init]"));

    UNREFERENCED_PARAMETER(ResourceList);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_UnknownAdapter = UnknownAdapter;
    m_UnknownAdapter->AddRef();

    m_Port = Port_;
    m_Port->AddRef();

    // The filter descriptor is specified in lamaloopbackcapture.h
    // No further initialization of pins/nodes needed here for passthrough.

    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackCapture::NewStream
//
// Creates a new stream.
//
NTSTATUS
CMiniportWaveRTLamaLoopbackCapture::NewStream
(
    _Out_ PMINIPORTWAVERTSTREAM * Stream,
    _In_  PPORTWAVERTSTREAM       PortStream,
    _In_  ULONG                   Pin,
    _In_  BOOLEAN                 Capture,
    _In_  PKSDATAFORMAT           DataFormat
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::NewStream]"));

    UNREFERENCED_PARAMETER(PortStream);
    UNREFERENCED_PARAMETER(Pin);
    UNREFERENCED_PARAMETER(Capture);
    UNREFERENCED_PARAMETER(DataFormat);

    *Stream = NULL;
    // For now, return an error as stream creation is not implemented.
    // A real driver would create a stream object (e.g., CMiniportWaveRTLamaLoopbackStream)
    return STATUS_NOT_IMPLEMENTED; 
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackCapture NonDelegatingQueryInterface
//
// Obtains an interface.
//
NTSTATUS
CMiniportWaveRTLamaLoopbackCapture::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * Object
)
{
    PAGED_CODE();
    ASSERT(Object);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::NonDelegatingQueryInterface]"));

    if (IsEqualGUIDAligned(Interface, IID_IUnknown) ||
        IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT))
    {
        *Object = PVOID(PMINIPORTWAVERT(this));
    }
    else
    {
        *Object = NULL;
        return STATUS_NOT_SUPPORTED; // Corrected
    }

    PUNKNOWN(*Object)->AddRef();
    return STATUS_SUCCESS;
}

//=============================================================================
// Factory function for Capture WaveRT Miniport
//=============================================================================
NTSTATUS
CreateMiniportWaveRTLamaLoopbackCapture
(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
)
{
    PAGED_CODE();
    ASSERT(Unknown);
    DPF_ENTER(("[CreateMiniportWaveRTLamaLoopbackCapture]"));

    UNREFERENCED_PARAMETER(UnknownOuter);

    CMiniportWaveRTLamaLoopbackCapture *obj = new (PoolType, MINIPORT_POOLTAG) CMiniportWaveRTLamaLoopbackCapture;
    if (NULL == obj)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj);
    (*Unknown)->AddRef(); // The caller expects a referenced object

    return STATUS_SUCCESS;
}

#pragma code_seg() // End PAGED_CODE segment
