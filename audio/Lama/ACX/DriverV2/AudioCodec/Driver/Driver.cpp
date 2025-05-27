/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Driver.cpp

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"
#include "cpp_utils.h" // This likely contained scope_exit, which is being removed.

#ifndef __INTELLISENSE__
#include "driver.tmh"
#endif

_Use_decl_annotations_
void AudioCodecDriverUnload(
    _In_ WDFDRIVER Driver
)
{
    PAGED_CODE();

    // WPP_CLEANUP is called from DriverEntry's cleanup path upon failure,
    // or by the system calling this DriverUnload normally.
    // Avoid double cleanup if DriverUnload is called after a failed DriverEntry.
    // However, WPP_CLEANUP is generally safe to call multiple times if WPP_INIT_TRACING succeeded.
    // The original scope_exit called WPP_CLEANUP *only on failure*.
    // AudioCodecDriverUnload is for the *success path* unload.
    // So, WPP_CLEANUP here is correct for normal unload.

    // The g_RegistryPath cleanup is also in DriverEntry's cleanup.
    // If DriverEntry fails after allocating g_RegistryPath, it's cleaned there.
    // If DriverEntry succeeds, this unload callback cleans it.
    if (g_RegistryPath.Buffer != NULL)
    {
        ExFreePool(g_RegistryPath.Buffer);
        RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath));
    }
    
    // WPP_CLEANUP is usually the last thing.
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));


    return;
}

INIT_CODE_SEG
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG           wdfCfg;
    ACX_DRIVER_CONFIG           acxCfg;
    WDFDRIVER                   driver = NULL; // Initialize to NULL
    NTSTATUS                    status = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES       attributes;

    PAGED_CODE();
    
    // WPP_INIT_TRACING should be called early.
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    // Ensure g_RegistryPath is initialized for cleanup logic
    // Note: g_RegistryPath is a global. If DriverEntry could be re-entered for the same driver load (highly unlikely for a standard driver),
    // this might need protection or be handled differently. Assuming standard single-entry.
    // The original scope_exit did not re-initialize g_RegistryPath.Buffer if already allocated,
    // it only freed it on failure. The unload routine handles freeing on success.
    // For safety in the cleanup block, ensure it's NULL before any allocation attempt.
    g_RegistryPath.Buffer = NULL; 
    g_RegistryPath.Length = 0;
    g_RegistryPath.MaximumLength = 0;

    status = CopyRegistrySettingsPath(RegistryPath);
    if (!NT_SUCCESS(status)) {
        // KdPrintEx for error: "LAMA: CopyRegistrySettingsPath failed %!STATUS!\n", status
        goto cleanup;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG_INIT(&wdfCfg, Codec_EvtBusDeviceAdd);
    wdfCfg.EvtDriverUnload = AudioCodecDriverUnload;

    //
    // Create a framework driver object to represent our driver.
    //
    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &wdfCfg, &driver);
    if (!NT_SUCCESS(status)) {
        // KdPrintEx for error: "LAMA: WdfDriverCreate failed %!STATUS!\n", status
        // 'driver' is not valid here, so no WDF object cleanup specific to 'driver' needed yet.
        goto cleanup;
    }

    //
    // Initializing the ACX driver configuration struct which contains size and flags
    // elements. 
    //
    ACX_DRIVER_CONFIG_INIT(&acxCfg);

    // 
    // The driver calls this DDI in its DriverEntry callback after creating the WDF driver
    // object. ACX uses this call to apply any post driver settings.
    //
    status = AcxDriverInitialize(driver, &acxCfg);
    if (!NT_SUCCESS(status)) {
        // KdPrintEx for error: "LAMA: AcxDriverInitialize failed %!STATUS!\n", status
        // If AcxDriverInitialize fails, WDF will still call AudioCodecDriverUnload during driver teardown,
        // which will handle WPP_CLEANUP. WdfDriverCreate succeeded, so the WDFDRIVER object exists.
        // The original scope_exit logic was simpler and didn't call AcxDriverUninitialize.
        // We are replicating the original scope_exit's cleanup items.
        goto cleanup;
    }

    // If all successful
    // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "LAMA: DriverEntry successful.\n"));
    return STATUS_SUCCESS; 

cleanup:
    // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: DriverEntry failed with status %!STATUS!\n", status));

    // This cleanup path is taken only if an operation *after* WPP_INIT_TRACING fails.
    if (g_RegistryPath.Buffer != NULL) { 
        ExFreePool(g_RegistryPath.Buffer);
        RtlZeroMemory(&g_RegistryPath, sizeof(g_RegistryPath)); 
    }

    // WPP_CLEANUP is called if WPP_INIT_TRACING was called.
    // If WdfDriverCreate failed, DriverObject is still valid.
    WPP_CLEANUP(DriverObject);

    return status; // Return the actual failure code
}

