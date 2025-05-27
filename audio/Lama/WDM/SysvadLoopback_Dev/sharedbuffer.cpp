#include "lamaloopbackcommon.h"
#include <ntstatus.h> // For STATUS_SUCCESS, etc.

#pragma code_seg("PAGE")

// Define the global array of shared loopback buffers
LAMA_SHARED_LOOPBACK_BUFFER g_InstanceLoopbackBuffers[MAX_LAMA_INSTANCES];

// Define Global Audio Format Variables
ULONG g_CurrentGlobalSampleRate    = DEFAULT_SAMPLE_RATE_PCM;
ULONG g_CurrentGlobalChannels      = DEFAULT_CHANNELS_PCM;
ULONG g_CurrentGlobalBitsPerSample = DEFAULT_BITS_PER_SAMPLE_PCM;


//-----------------------------------------------------------------------------
// InitializeAllSharedLoopbackBuffers
//
// Allocates and initializes all per-instance shared loopback buffers.
// This function should be called once, e.g., from DriverEntry.
//-----------------------------------------------------------------------------
NTSTATUS
InitializeAllSharedLoopbackBuffers
(
    VOID
)
{
    PAGED_CODE();
    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN bAllSucceeded = TRUE;

    DPF(DPF_LEVEL_INFO, ("Initializing all shared loopback buffers..."));

    for (ULONG i = 0; i < MAX_LAMA_INSTANCES; ++i)
    {
        if (g_InstanceLoopbackBuffers[i].bInitialized)
        {
            DPF(DPF_LEVEL_INFO, ("Shared loopback buffer for instance %u already initialized.", i));
            continue;
        }

        DPF(DPF_LEVEL_INFO, ("Initializing shared loopback buffer for instance %u...", i));

        g_InstanceLoopbackBuffers[i].pBuffer = (PBYTE)ExAllocatePoolWithTag(
                                            NonPagedPoolNx, 
                                            LAMA_SHARED_RING_BUFFER_SIZE,
                                            LAMA_POOL_TAG
                                            );

        if (!g_InstanceLoopbackBuffers[i].pBuffer)
        {
            DPF(DPF_LEVEL_ERROR, ("Failed to allocate shared loopback buffer for instance %u.", i));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            g_InstanceLoopbackBuffers[i].bInitialized = FALSE; // Ensure it's marked as not initialized
            bAllSucceeded = FALSE;
            continue; // Try to initialize other buffers
        }

        RtlZeroMemory(g_InstanceLoopbackBuffers[i].pBuffer, LAMA_SHARED_RING_BUFFER_SIZE);

        g_InstanceLoopbackBuffers[i].ulBufferSize = LAMA_SHARED_RING_BUFFER_SIZE;
        g_InstanceLoopbackBuffers[i].ulWritePointer = 0;
        g_InstanceLoopbackBuffers[i].ulReadPointer = 0;
        KeInitializeSpinLock(&g_InstanceLoopbackBuffers[i].SpinLock);
        g_InstanceLoopbackBuffers[i].bInitialized = TRUE;

        DPF(DPF_LEVEL_INFO, ("Shared loopback buffer for instance %u initialized successfully. Size: %u bytes", i, LAMA_SHARED_RING_BUFFER_SIZE));
    }

    if (!bAllSucceeded && ntStatus == STATUS_SUCCESS) // Should only happen if some were pre-initialized and others failed
    {
        // If at least one new allocation failed, reflect that.
        // If all were pre-initialized, ntStatus remains SUCCESS.
        // If some were pre-init, and new ones succeeded, also SUCCESS.
        // This logic ensures if any *new* initialization fails, we return an error.
        // However, if the loop completes and ntStatus is still success (meaning no *new* failures),
        // but bAllSucceeded is false (meaning some *new* buffers failed, or some were already init and others failed),
        // we should return the error from the failed ones.
        // The current ntStatus will hold the error from the last failure, or SUCCESS if all new ones worked.
        // If ntStatus is SUCCESS but bAllSucceeded is FALSE, it implies some buffers failed to initialize
        // but they weren't the *last* one attempted, or some were already initialized.
        // For simplicity, if not all are initialized (either new or pre-existing), it's an issue.
        // A better approach might be to free successfully allocated ones if any fails.
        // For now, we signal error if any instance is not initialized properly.
        
        // Check if all are truly initialized now.
        BOOLEAN allCurrentlyInitialized = TRUE;
        for (ULONG i = 0; i < MAX_LAMA_INSTANCES; ++i) {
            if (!g_InstanceLoopbackBuffers[i].bInitialized) {
                allCurrentlyInitialized = FALSE;
                break;
            }
        }
        if (!allCurrentlyInitialized) {
             DPF(DPF_LEVEL_ERROR, ("Not all shared loopback buffers could be initialized."));
             // ntStatus should already be STATUS_INSUFFICIENT_RESOURCES if an ExAllocatePoolWithTag failed.
             // If it's STATUS_SUCCESS here, it implies a logic error or that all failures were on already-init buffers (which is fine).
             // We will return the ntStatus from the last ExAllocatePoolWithTag failure, or SUCCESS if all were fine/pre-init.
        }
    }
    
    if (NT_SUCCESS(ntStatus) && !bAllSucceeded) {
        // This case means some allocations failed, but the last operation was successful (e.g. skipped an already init buffer)
        // or the loop finished. We should indicate overall failure.
        ntStatus = STATUS_INSUFFICIENT_RESOURCES; // Generic error if some failed.
    }


    DPF(DPF_LEVEL_INFO, ("Initial global format: SR=%u, CH=%u, Bits=%u", 
        g_CurrentGlobalSampleRate, g_CurrentGlobalChannels, g_CurrentGlobalBitsPerSample));

    return ntStatus;
}

//-----------------------------------------------------------------------------
// FreeAllSharedLoopbackBuffers
//
// Frees all per-instance shared loopback buffers.
// This function should be called once when the driver is unloading.
//-----------------------------------------------------------------------------
VOID
FreeAllSharedLoopbackBuffers
(
    VOID
)
{
    PAGED_CODE();

    DPF(DPF_LEVEL_INFO, ("Freeing all shared loopback buffers..."));

    for (ULONG i = 0; i < MAX_LAMA_INSTANCES; ++i)
    {
        if (!g_InstanceLoopbackBuffers[i].bInitialized)
        {
            DPF(DPF_LEVEL_INFO, ("Shared loopback buffer for instance %u was not initialized or already freed.", i));
            continue;
        }

        DPF(DPF_LEVEL_INFO, ("Freeing shared loopback buffer for instance %u...", i));

        if (g_InstanceLoopbackBuffers[i].pBuffer)
        {
            ExFreePoolWithTag(g_InstanceLoopbackBuffers[i].pBuffer, LAMA_POOL_TAG);
            g_InstanceLoopbackBuffers[i].pBuffer = NULL;
        }

        g_InstanceLoopbackBuffers[i].ulBufferSize = 0;
        g_InstanceLoopbackBuffers[i].ulWritePointer = 0;
        g_InstanceLoopbackBuffers[i].ulReadPointer = 0;
        g_InstanceLoopbackBuffers[i].bInitialized = FALSE;
        // Spinlock doesn't need de-initialization.
    }

    DPF(DPF_LEVEL_INFO, ("All shared loopback buffers freed."));
}

#pragma code_seg()
