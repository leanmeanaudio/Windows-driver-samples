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
// Make sure we have struct keyword before the type to avoid missing semicolon errors
extern const struct _MINIFILTER_DESCRIPTOR LamaLoopbackRenderTopoMiniportFilterDescriptor;
extern const struct _MINIFILTER_DESCRIPTOR LamaLoopbackCaptureTopoMiniportFilterDescriptor;

//
// Forward declare the Physical Connection Tables defined in lamaloopbacktopo.cpp
//
// Use struct to avoid incomplete type errors
extern const struct _PHYSICALCONNECTIONTABLE LamaLoopbackRenderTopologyPhysicalConnections[];
extern const struct _PHYSICALCONNECTIONTABLE LamaLoopbackCaptureTopologyPhysicalConnections[];

#endif // _SYSVAD_LAMALOOPBACKTOPTABLE_H_
