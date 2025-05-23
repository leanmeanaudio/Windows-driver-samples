#include "lamaloopbackcommon.h"
#include <ntstatus.h> // For STATUS_SUCCESS, etc.

#pragma code_seg("PAGE")

// Define the global shared loopback buffer instance
LAMA_SHARED_LOOPBACK_BUFFER g_SharedLoopbackBuffer = {0}; // Initialize to zero

// Define Global Audio Format Variables
// These are initialized to default values from lamaloopbackcommon.h
// KSPROPERTY_LAMA_SAMPLE_RATE will modify g_CurrentGlobalSampleRate.
// Others are here for consistency if the device needs to globally track these.
ULONG g_CurrentGlobalSampleRate    = DEFAULT_SAMPLE_RATE_PCM;
ULONG g_CurrentGlobalChannels      = DEFAULT_CHANNELS_PCM;
ULONG g_CurrentGlobalBitsPerSample = DEFAULT_BITS_PER_SAMPLE_PCM;


//-----------------------------------------------------------------------------
// InitializeSharedLoopbackBuffer
//
// Allocates and initializes the global shared loopback buffer.
// This function should ideally be called once, e.g., from DriverEntry or
// the AddDevice function of the driver.
//-----------------------------------------------------------------------------
NTSTATUS
InitializeSharedLoopbackBuffer
(
    _In_opt_ PDEVICE_OBJECT DeviceObject // Optional: for context, can be NULL
)
{
    PAGED_CODE();

    if (g_SharedLoopbackBuffer.bInitialized)
    {
        DPF(DPF_LEVEL_INFO, ("Shared loopback buffer already initialized."));
        return STATUS_SUCCESS;
    }

    DPF(DPF_LEVEL_INFO, ("Initializing shared loopback buffer..."));

    g_SharedLoopbackBuffer.pBuffer = (PBYTE)ExAllocatePoolWithTag(
                                        NonPagedPoolNx, 
                                        LAMA_SHARED_RING_BUFFER_SIZE,
                                        LAMA_POOL_TAG
                                        );

    if (!g_SharedLoopbackBuffer.pBuffer)
    {
        DPF(DPF_LEVEL_ERROR, ("Failed to allocate shared loopback buffer."));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_SharedLoopbackBuffer.pBuffer, LAMA_SHARED_RING_BUFFER_SIZE);

    g_SharedLoopbackBuffer.ulBufferSize = LAMA_SHARED_RING_BUFFER_SIZE;
    g_SharedLoopbackBuffer.ulWritePointer = 0;
    g_SharedLoopbackBuffer.ulReadPointer = 0;
    KeInitializeSpinLock(&g_SharedLoopbackBuffer.SpinLock);
    g_SharedLoopbackBuffer.pAssociatedDeviceObject = DeviceObject; 
    g_SharedLoopbackBuffer.bInitialized = TRUE;

    DPF(DPF_LEVEL_INFO, ("Shared loopback buffer initialized successfully. Size: %u bytes", LAMA_SHARED_RING_BUFFER_SIZE));
    DPF(DPF_LEVEL_INFO, ("Initial global format: SR=%u, CH=%u, Bits=%u", 
        g_CurrentGlobalSampleRate, g_CurrentGlobalChannels, g_CurrentGlobalBitsPerSample));

    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// FreeSharedLoopbackBuffer
//
// Frees the global shared loopback buffer.
// This function should be called once when the driver is unloading.
//-----------------------------------------------------------------------------
VOID
FreeSharedLoopbackBuffer
(
    VOID
)
{
    PAGED_CODE();

    if (!g_SharedLoopbackBuffer.bInitialized)
    {
        DPF(DPF_LEVEL_INFO, ("Shared loopback buffer was not initialized or already freed."));
        return;
    }

    DPF(DPF_LEVEL_INFO, ("Freeing shared loopback buffer..."));

    if (g_SharedLoopbackBuffer.pBuffer)
    {
        ExFreePoolWithTag(g_SharedLoopbackBuffer.pBuffer, LAMA_POOL_TAG);
        g_SharedLoopbackBuffer.pBuffer = NULL;
    }

    g_SharedLoopbackBuffer.ulBufferSize = 0;
    g_SharedLoopbackBuffer.ulWritePointer = 0;
    g_SharedLoopbackBuffer.ulReadPointer = 0;
    g_SharedLoopbackBuffer.bInitialized = FALSE;
    g_SharedLoopbackBuffer.pAssociatedDeviceObject = NULL;

    DPF(DPF_LEVEL_INFO, ("Shared loopback buffer freed."));
}

#pragma code_seg()
