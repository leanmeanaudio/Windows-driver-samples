//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#ifndef _SYSVAD_LAMALOOPBACKTOPTABLE_H_
#define _SYSVAD_LAMALOOPBACKTOPTABLE_H_

#include "sysvad.h"   // For MINIFILTER_DESCRIPTOR, PHYSICALCONNECTIONTABLE etc.
#include <portcls.h>

//
// Forward declare the Topology Miniport Filter Descriptors defined in lamaloopbacktopo.cpp
//
extern const MINIFILTER_DESCRIPTOR LamaLoopbackRenderTopoMiniportFilterDescriptor;
extern const MINIFILTER_DESCRIPTOR LamaLoopbackCaptureTopoMiniportFilterDescriptor;

//
// Forward declare the Physical Connection Tables defined in lamaloopbacktopo.cpp
//
extern const PHYSICALCONNECTIONTABLE LamaLoopbackRenderTopologyPhysicalConnections[];
extern const PHYSICALCONNECTIONTABLE LamaLoopbackCaptureTopologyPhysicalConnections[];

#endif // _SYSVAD_LAMALOOPBACKTOPTABLE_H_
