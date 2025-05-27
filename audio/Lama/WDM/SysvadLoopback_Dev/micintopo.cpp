// Include our audio driver compatibility header first
#include "audio_compat.h"

// Compatibility fix for portcls.h must be first include
#include "portcls_compat.h"

// Include our direct fix header at the very beginning
#include "direct_fix.h"

// Include our unified compatibility header first
#include "sysvad_compat.h"

// Standard Windows headers
#include <portcls.h>
#include "ksdataformat_forward.h"
#include "lamaloopbackcommon.h" // For globals, property GUIDs, common formats, etc.
#include "micintopo.h"
#include "lamaloopbackrender.h" // Include for full class definitions

// Using LAMA_POOL_TAG from lamaloopbackcommon.h

#pragma code_seg("PAGE")
//=============================================================================
// Factory Methods
//=============================================================================

// The CreateMiniportTopologyLamaLoopbackCapture function creates a new instance of the
// CMiniportTopologyLamaLoopbackCapture class that is defined in lamaloopbackrender.h
NTSTATUS CreateMiniportTopologyLamaLoopbackCapture
( _Out_ PUNKNOWN * Unknown, _In_ REFCLSID RefClsId, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{
    PAGED_CODE(); ASSERT(Unknown);
    DbgPrint("Entered CreateMiniportTopologyLamaLoopbackCapture\n");
    UNREFERENCED_PARAMETER(RefClsId);
    UNREFERENCED_PARAMETER(UnknownOuter);
    
    // Get the appropriate filter descriptor
    static PCFILTER_DESCRIPTOR MicInTopoFilterDesc = NULL;
    
    // Create the miniport with the proper parameters - note we need to use standard allocation
    // and can't use the placement new syntax with PoolType directly
    CMiniportTopologyLamaLoopbackCapture *obj = new CMiniportTopologyLamaLoopbackCapture(
        UnknownOuter,
        &MicInTopoFilterDesc,
        2,  // DeviceMaxChannels - typical for microphone
        eMicArrayDevice1,  // DeviceType - use a valid device type
        NULL  // DeviceContext - no specific device context needed
    );
    
    if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; }
    
    *Unknown = PUNKNOWN(PMINIPORTTOPOLOGY(obj));
    (*Unknown)->AddRef();
    
    return STATUS_SUCCESS;
}

// The CreateMiniportWaveRTLamaLoopbackCapture function creates a new instance of the
// CMiniportWaveRTLamaLoopbackCapture class that is defined in lamaloopbackrender.h
NTSTATUS CreateMiniportWaveRTLamaLoopbackCapture
( _Out_ PUNKNOWN * Unknown, _In_ REFCLSID RefClsId, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{
    PAGED_CODE(); ASSERT(Unknown);
    DbgPrint("Entered CreateMiniportWaveRTLamaLoopbackCapture\n");
    UNREFERENCED_PARAMETER(RefClsId);
    UNREFERENCED_PARAMETER(UnknownOuter);
    
    // Create a dummy ENDPOINT_MINIPAIR structure for the capture device
    ENDPOINT_MINIPAIR miniportPair;
    RtlZeroMemory(&miniportPair, sizeof(ENDPOINT_MINIPAIR));
    miniportPair.DeviceType = eMicArrayDevice1;
    miniportPair.DeviceMaxChannels = 2; // Typical for capture device
    
    // Create the miniport with the proper parameters - use standard allocation
    CMiniportWaveRTLamaLoopbackCapture *obj = new CMiniportWaveRTLamaLoopbackCapture(
        UnknownOuter,  // Using UnknownOuter as the adapter
        &miniportPair, // Providing minimal endpoint pair info
        NULL           // No specific device context
    );
    
    if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; }
    
    *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj);
    (*Unknown)->AddRef();
    
    return STATUS_SUCCESS;
}

#pragma code_seg() // End PAGED_CODE segment





