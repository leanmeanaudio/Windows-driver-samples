#include <ntddk.h>
#include <wdm.h>
#include <portcls.h>
#include "lamaloopbackrender.h"
#include "baseaddress.h" // For DPF_ENTER etc.

#pragma code_seg("PAGE")
//=============================================================================
// CMiniportTopologyLamaLoopbackRender
//=============================================================================

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender
//
// Constructor for the topology miniport.
//
CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender
(
    void
)
:   CUnknown(NULL),
    m_Port(NULL),
    m_UnknownAdapter(NULL)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender]"));
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender
//
// Destructor for the topology miniport.
//
CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender
(
    void
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender]"));

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
// CMiniportTopologyLamaLoopbackRender::Init
//
// Initializes the topology miniport.
//
NTSTATUS
CMiniportTopologyLamaLoopbackRender::Init
(
    _In_  PUNKNOWN        UnknownAdapter,
    _In_  PRESOURCELIST   ResourceList,
    _In_  PPORTTOPOLOGY   Port_
)
{
    PAGED_CODE();
    ASSERT(UnknownAdapter);
    ASSERT(Port_);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::Init]"));

    UNREFERENCED_PARAMETER(ResourceList);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_UnknownAdapter = UnknownAdapter;
    m_UnknownAdapter->AddRef();

    m_Port = Port_;
    m_Port->AddRef();

    // The filter descriptor is specified in lamaloopbackrender.h
    // No further initialization of pins/nodes needed here for passthrough.

    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportTopologyLamaLoopbackRender::DataRangeIntersection
//
// Handles data range intersection queries.
// This is a simplified version. A real driver would need to check PinId
// and offer appropriate formats.
//
NTSTATUS
CMiniportTopologyLamaLoopbackRender::DataRangeIntersection
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
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::DataRangeIntersection]"));

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
// CMiniportTopologyLamaLoopbackRender NonDelegatingQueryInterface
//
// Obtains an interface.
//
NTSTATUS
CMiniportTopologyLamaLoopbackRender::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * Object
)
{
    PAGED_CODE();
    ASSERT(Object);
    DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::NonDelegatingQueryInterface]"));

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
        return STATUS_NOT_SUPPORTED; // Corrected from STATUS_INVALID_PARAMETER
    }

    PUNKNOWN(*Object)->AddRef();
    return STATUS_SUCCESS;
}

//=============================================================================
// Factory function for Render Topology Miniport
//=============================================================================
NTSTATUS
CreateMiniportTopologyLamaLoopbackRender
(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
)
{
    PAGED_CODE();
    ASSERT(Unknown);
    DPF_ENTER(("[CreateMiniportTopologyLamaLoopbackRender]"));

    UNREFERENCED_PARAMETER(UnknownOuter);

    CMiniportTopologyLamaLoopbackRender *obj = new (PoolType, MINIPORT_POOLTAG) CMiniportTopologyLamaLoopbackRender;
    if (NULL == obj)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    *Unknown = PUNKNOWN((PMINIPORTTOPOLOGY)obj);
    (*Unknown)->AddRef(); // The caller expects a referenced object

    return STATUS_SUCCESS;
}


//=============================================================================
// CMiniportWaveRTLamaLoopbackRender
//=============================================================================

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender
//
// Constructor for the wave miniport.
//
CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender
(
    void
)
:   CUnknown(NULL),
    m_Port(NULL),
    m_UnknownAdapter(NULL)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender]"));
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender
//
// Destructor for the wave miniport.
//
CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender
(
    void
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender]"));

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
// CMiniportWaveRTLamaLoopbackRender::Init
//
// Initializes the wave miniport.
//
NTSTATUS
CMiniportWaveRTLamaLoopbackRender::Init
(
    _In_  PUNKNOWN        UnknownAdapter,
    _In_  PRESOURCELIST   ResourceList,
    _In_  PPORTWAVERT     Port_
)
{
    PAGED_CODE();
    ASSERT(UnknownAdapter);
    ASSERT(Port_);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::Init]"));

    UNREFERENCED_PARAMETER(ResourceList);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_UnknownAdapter = UnknownAdapter;
    m_UnknownAdapter->AddRef();

    m_Port = Port_;
    m_Port->AddRef();

    // The filter descriptor is specified in lamaloopbackrender.h
    // No further initialization of pins/nodes needed here for passthrough.

    return ntStatus;
}

//-----------------------------------------------------------------------------
// CMiniportWaveRTLamaLoopbackRender::NewStream
//
// Creates a new stream.
//
NTSTATUS
CMiniportWaveRTLamaLoopbackRender::NewStream
(
    _Out_ PMINIPORTWAVERTSTREAM * Stream,
    _In_  PPORTWAVERTSTREAM       PortStream,
    _In_  ULONG                   Pin,
    _In_  BOOLEAN                 Capture,
    _In_  PKSDATAFORMAT           DataFormat
)
{
    PAGED_CODE();
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::NewStream]"));

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
// CMiniportWaveRTLamaLoopbackRender NonDelegatingQueryInterface
//
// Obtains an interface.
//
NTSTATUS
CMiniportWaveRTLamaLoopbackRender::NonDelegatingQueryInterface
(
    _In_         REFIID  Interface,
    _COM_Outptr_ PVOID * Object
)
{
    PAGED_CODE();
    ASSERT(Object);
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::NonDelegatingQueryInterface]"));

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
// Factory function for Render WaveRT Miniport
//=============================================================================
NTSTATUS
CreateMiniportWaveRTLamaLoopbackRender
(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
)
{
    PAGED_CODE();
    ASSERT(Unknown);
    DPF_ENTER(("[CreateMiniportWaveRTLamaLoopbackRender]"));

    UNREFERENCED_PARAMETER(UnknownOuter);

    CMiniportWaveRTLamaLoopbackRender *obj = new (PoolType, MINIPORT_POOLTAG) CMiniportWaveRTLamaLoopbackRender;
    if (NULL == obj)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj);
    (*Unknown)->AddRef(); // The caller expects a referenced object

    return STATUS_SUCCESS;
}

#pragma code_seg() // End PAGED_CODE segment
