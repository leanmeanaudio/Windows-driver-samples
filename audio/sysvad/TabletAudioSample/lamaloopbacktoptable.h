//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#ifndef _SYSVAD_LAMALOOPBACKTOPTABLE_H_
#define _SYSVAD_LAMALOOPBACKTOPTABLE_H_

#include <portcls.h> // For MINIFILTER_DESCRIPTOR
#include "sysvad.h"   // For PHYSICALCONNECTIONTABLE (assuming it's defined here or in included headers)

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
