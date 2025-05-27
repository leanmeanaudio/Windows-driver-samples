//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#ifndef _SYSVAD_LAMALOOPBACKTOPO_H_
#define _SYSVAD_LAMALOOPBACKTOPO_H_

#include <portcls.h>
#include <ksmedia.h>
#include "sysvad.h" // For MINIFILTER_DESCRIPTOR, PHYSICALCONNECTIONTABLE, etc.

// Replace "endpointscommon.h" with direct include to fix path issues
#include "..\EndpointsCommon\basetopo.h" // For node types and miniport topology functions

// Pin definitions for topology miniport
#define KSPIN_TOPO_WAVEIN_SOURCE      0       // from wave filter
#define KSPIN_TOPO_BRIDGE             1       // to wave filter

// Node definitions for topology miniport
#define KSNODE_TOPO_SUM               0       // Sum node

//=============================================================================
// Defines
//=============================================================================
#define LAMA_LOOPBACK_TOPO_PIN_COUNT_RENDER 1
#define LAMA_LOOPBACK_TOPO_NODE_COUNT_RENDER 1
#define LAMA_LOOPBACK_TOPO_CONNECTION_COUNT_RENDER 2

#define LAMA_LOOPBACK_TOPO_PIN_COUNT_CAPTURE 1
#define LAMA_LOOPBACK_TOPO_NODE_COUNT_CAPTURE 1
#define LAMA_LOOPBACK_TOPO_CONNECTION_COUNT_CAPTURE 2

//=============================================================================
// Externs (defined in lamaloopbacktopo.cpp)
//=============================================================================

//
// Render Topology
//
extern const KSPIN_DESCRIPTOR LamaLoopbackRender_TopoPins[LAMA_LOOPBACK_TOPO_PIN_COUNT_RENDER];
extern const KSNODE_DESCRIPTOR LamaLoopbackRender_TopoNodes[LAMA_LOOPBACK_TOPO_NODE_COUNT_RENDER];
extern const KSPIN_CONNECT LamaLoopbackRender_TopoConnections[LAMA_LOOPBACK_TOPO_CONNECTION_COUNT_RENDER];
extern const PCAUTOMATION_TABLE LamaLoopbackRender_AutomationTable;

//
// Capture Topology
//
extern const KSPIN_DESCRIPTOR LamaLoopbackCapture_TopoPins[LAMA_LOOPBACK_TOPO_PIN_COUNT_CAPTURE];
extern const KSNODE_DESCRIPTOR LamaLoopbackCapture_TopoNodes[LAMA_LOOPBACK_TOPO_NODE_COUNT_CAPTURE];
extern const KSPIN_CONNECT LamaLoopbackCapture_TopoConnections[LAMA_LOOPBACK_TOPO_CONNECTION_COUNT_CAPTURE];
extern const PCAUTOMATION_TABLE LamaLoopbackCapture_AutomationTable;

//
// Physical Connections (defined in lamaloopbacktopo.cpp, used by lamaloopbackminipairs.h)
//
extern const PHYSICALCONNECTIONTABLE LamaLoopbackRenderTopologyPhysicalConnections[];
extern const PHYSICALCONNECTIONTABLE LamaLoopbackCaptureTopologyPhysicalConnections[];

//
// Topology Miniport Filter Descriptors (defined in lamaloopbacktopo.cpp)
//
extern const MINIFILTER_DESCRIPTOR LamaLoopbackRenderTopoMiniportFilterDescriptor;
extern const MINIFILTER_DESCRIPTOR LamaLoopbackCaptureTopoMiniportFilterDescriptor;

#endif // _SYSVAD_LAMALOOPBACKTOPO_H_
