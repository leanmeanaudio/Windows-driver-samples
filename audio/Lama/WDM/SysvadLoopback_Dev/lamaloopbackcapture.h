#pragma once

// This file provides declarations for the Lama Loopback Capture functionality
// Most of the capture-related declarations are already defined in lamaloopbackrender.h
#include "lamaloopbackrender.h"

// Any additional capture-specific declarations would go here

// Function declarations (these are already in lamaloopbackrender.h but explicitly declared here for clarity)
NTSTATUS
CreateMiniportTopologyLamaLoopbackCapture(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID RefId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);

NTSTATUS
CreateMiniportWaveRTLamaLoopbackCapture(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID RefId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);
