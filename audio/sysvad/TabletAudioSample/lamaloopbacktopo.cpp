//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <ntddk.h>
#include <portcls.h>
#include <ksmedia.h>         // For KSCATEGORY_AUDIO, KSNODETYPE_SUM, etc.
#include "sysvad.h"          // For MINIFILTER_DESCRIPTOR, PHYSICALCONNECTIONTABLE, PinDataRangesBridge, CONNECTIONTYPE_*, etc. (assumed to resolve to ../common/sysvad.h)
// Removed: #include "endpointscommon.h"
// Added specific include from EndpointsCommon:
#include "..\EndpointsCommon\basetopo.h" // For MiniportTopologySimpleAutomation, CreateMiniportTopologySYSVAD
#include "lamaloopbacktopo.h"

//=============================================================================
// Render Topology Definitions
//=============================================================================

//
// Pin descriptors
//
static const KSPIN_DESCRIPTOR LamaLoopbackRender_TopoPins[] =
{
    // KSPIN_TOPO_WAVEIN_SOURCE (Input from Wave Miniport)
    {
        0, // InterfacesCount
        NULL, // Interfaces
        0, // MediumsCount
        NULL, // Mediums
        SIZEOF_ARRAY(PinDataRangesBridge), // DataRangesCount (from sysvad.h -> common.h)
        PinDataRangesBridge, // DataRanges (from sysvad.h -> common.h)
        KSPIN_DATAFLOW_IN, // DataFlow
        KSPIN_COMMUNICATION_SINK, // Communication
        &KSCATEGORY_AUDIO, // Category (from ksmedia.h)
        NULL, // Name
        0  // ConstrainedDataRangesCount
    }
};

//
// Node descriptors
//
static const KSNODE_DESCRIPTOR LamaLoopbackRender_TopoNodes[] =
{
    // KSNODE_TOPO_SUM
    {
        NULL, // AutomationTable
        &KSNODETYPE_SUM, // Type (from ksmedia.h)
        NULL // Name
    }
};

//
// Connection descriptors
//
// Topology layout for render:
// KSPIN_TOPO_WAVEIN_SOURCE -> SUM (Node 0) -> KSPIN_TOPO_BRIDGE
//
static const KSPIN_CONNECT LamaLoopbackRender_TopoConnections[] =
{
    // From KSPIN_TOPO_WAVEIN_SOURCE (Pin 0) to SUM Node (Node 0) In (Pin 1)
    {
        KSPIN_TOPO_WAVEIN_SOURCE,   // FromNode (Pin)
        0,                          // FromPin (Pin Index)
        0,                          // ToNode (Node Index)
        1                           // ToPin (Node Pin Index - SUM node typically has multiple inputs, 0 is output)
    },
    // From SUM Node (Node 0) Out (Pin 0) to KSPIN_TOPO_BRIDGE
    {
        0,                          // FromNode (Node Index)
        0,                          // FromPin (Node Pin Index - SUM node output is 0)
        KSPIN_TOPO_BRIDGE,          // ToNode (Bridge Pin)
        0                           // ToPin (Bridge Pin Index - typically 0)
    }
};

//
// Automation table
//
static const PCAUTOMATION_TABLE LamaLoopbackRender_AutomationTable = &MiniportTopologySimpleAutomation; // From basetopo.h

//
// Physical Connections Table
//
const PHYSICALCONNECTIONTABLE LamaLoopbackRenderTopologyPhysicalConnections[] = // PHYSICALCONNECTIONTABLE from sysvad.h
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn (Connects to Wave Out's bridge pin) - this is from perspective of wave miniport
        KSPIN_WAVE_RENDER_SOURCE,   // WaveOut (The bridge pin on the Wave miniport)
        CONNECTIONTYPE_WAVE_OUTPUT  // CONNECTIONTYPE_WAVE_OUTPUT from sysvad.h
    }
};

//
// Topology Miniport Filter Descriptor
//
const MINIFILTER_DESCRIPTOR LamaLoopbackRenderTopoMiniportFilterDescriptor = // MINIFILTER_DESCRIPTOR from sysvad.h
{
    MINIFILTER_DESCRIPTOR_FLAGS_VERSION, // FlagsVersion
    &LamaLoopbackRender_AutomationTable, // AutomationTable
    sizeof(KSPIN_DESCRIPTOR), // PinSize
    LAMA_LOOPBACK_TOPO_PIN_COUNT_RENDER, // PinCount
    LamaLoopbackRender_TopoPins, // Pins
    sizeof(KSNODE_DESCRIPTOR), // NodeSize
    LAMA_LOOPBACK_TOPO_NODE_COUNT_RENDER, // NodeCount
    LamaLoopbackRender_TopoNodes, // Nodes
    sizeof(KSPIN_CONNECT), // ConnectionSize
    LAMA_LOOPBACK_TOPO_CONNECTION_COUNT_RENDER, // ConnectionCount
    LamaLoopbackRender_TopoConnections, // Connections
    0, // CategoryCount
    NULL, // Categories
    CreateMiniportTopologySYSVAD, // MiniportCreate (from basetopo.h)
    DEFINE_KSFILTER_DESCRIPTOR(NULL) // Name (PortCls uses this for the symbolic link)
};

//=============================================================================
// Capture Topology Definitions
//=============================================================================

//
// Pin descriptors
//
static const KSPIN_DESCRIPTOR LamaLoopbackCapture_TopoPins[] =
{
    // KSPIN_TOPO_LINEOUT_DEST (Output to Wave Miniport)
    {
        0, // InterfacesCount
        NULL, // Interfaces
        0, // MediumsCount
        NULL, // Mediums
        SIZEOF_ARRAY(PinDataRangesBridge), // DataRangesCount (from sysvad.h -> common.h)
        PinDataRangesBridge, // DataRanges (from sysvad.h -> common.h)
        KSPIN_DATAFLOW_OUT, // DataFlow
        KSPIN_COMMUNICATION_SOURCE, // Communication
        &KSCATEGORY_AUDIO, // Category (from ksmedia.h)
        NULL, // Name
        0 // ConstrainedDataRangesCount
    }
};

//
// Node descriptors
//
static const KSNODE_DESCRIPTOR LamaLoopbackCapture_TopoNodes[] =
{
    // KSNODE_TOPO_SUM
    {
        NULL, // AutomationTable
        &KSNODETYPE_SUM, // Type (from ksmedia.h)
        NULL // Name
    }
};

//
// Connection descriptors
//
// Topology layout for capture:
// KSPIN_TOPO_BRIDGE -> SUM (Node 0) -> KSPIN_TOPO_LINEOUT_DEST
//
static const KSPIN_CONNECT LamaLoopbackCapture_TopoConnections[] =
{
    // From KSPIN_TOPO_BRIDGE to SUM Node (Node 0) In (Pin 1)
    {
        KSPIN_TOPO_BRIDGE,          // FromNode (Bridge Pin)
        0,                          // FromPin (Bridge Pin Index - typically 0)
        0,                          // ToNode (Node Index)
        1                           // ToPin (Node Pin Index - SUM node typically has multiple inputs, 0 is output)
    },
    // From SUM Node (Node 0) Out (Pin 0) to KSPIN_TOPO_LINEOUT_DEST (Pin 0)
    {
        0,                          // FromNode (Node Index)
        0,                          // FromPin (Node Pin Index - SUM node output is 0)
        KSPIN_TOPO_LINEOUT_DEST,    // ToNode (Pin)
        0                           // ToPin (Pin Index)
    }
};

//
// Automation table
//
static const PCAUTOMATION_TABLE LamaLoopbackCapture_AutomationTable = &MiniportTopologySimpleAutomation; // From basetopo.h

//
// Physical Connections Table
//
const PHYSICALCONNECTIONTABLE LamaLoopbackCaptureTopologyPhysicalConnections[] = // PHYSICALCONNECTIONTABLE from sysvad.h
{
    {
        KSPIN_TOPO_BRIDGE,          // TopologyOut (Connects to Wave In's bridge pin) - this is from perspective of wave miniport
        KSPIN_WAVE_BRIDGE,          // WaveIn (The bridge pin on the Wave miniport)
        CONNECTIONTYPE_TOPOLOGY_OUTPUT // CONNECTIONTYPE_TOPOLOGY_OUTPUT from sysvad.h
    }
};

//
// Topology Miniport Filter Descriptor
//
const MINIFILTER_DESCRIPTOR LamaLoopbackCaptureTopoMiniportFilterDescriptor = // MINIFILTER_DESCRIPTOR from sysvad.h
{
    MINIFILTER_DESCRIPTOR_FLAGS_VERSION, // FlagsVersion
    &LamaLoopbackCapture_AutomationTable, // AutomationTable
    sizeof(KSPIN_DESCRIPTOR), // PinSize
    LAMA_LOOPBACK_TOPO_PIN_COUNT_CAPTURE, // PinCount
    LamaLoopbackCapture_TopoPins, // Pins
    sizeof(KSNODE_DESCRIPTOR), // NodeSize
    LAMA_LOOPBACK_TOPO_NODE_COUNT_CAPTURE, // NodeCount
    LamaLoopbackCapture_TopoNodes, // Nodes
    sizeof(KSPIN_CONNECT), // ConnectionSize
    LAMA_LOOPBACK_TOPO_CONNECTION_COUNT_CAPTURE, // ConnectionCount
    LamaLoopbackCapture_TopoConnections, // Connections
    0, // CategoryCount
    NULL, // Categories
    CreateMiniportTopologySYSVAD, // MiniportCreate (from basetopo.h)
    DEFINE_KSFILTER_DESCRIPTOR(NULL) // Name (PortCls uses this for the symbolic link)
};
