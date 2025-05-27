/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Device.cpp - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include <wdm.h>
#include <windef.h>
#include "public.h" // Contains CODEC_DEVICE_CONTEXT definition
#include <devguid.h>
#include <wdmguid.h> 
#include <ks.h>
#include <mmsystem.h>
#include <ksmedia.h>
#include "streamengine.h"
#include "DriverSettings.h"
#include "LAMAConnectShared.h" // Contains LAMA_CONNECT_SHARED_BUFFER definition

#ifndef __INTELLISENSE__
#include "device.tmh"
#endif

// Declare the symbolic link name for LAMAConnect device interface
DECLARE_CONST_UNICODE_STRING(symbolicLinkName, L"\\DosDevices\\LAMAConnect0");

UNICODE_STRING g_RegistryPath = { 0 };      // This is used to store the registry settings path for the driver

ULONG DeviceDriverTag = DRIVER_TAG;

ULONG IdleTimeoutMsec = IDLE_TIMEOUT_MSEC;

// Forward declaration of the IOCTL handler
VOID Codec_EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
);

__drv_requiresIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
CopyRegistrySettingsPath(
    _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

Copies the following registry path to a global variable.

\REGISTRY\MACHINE\SYSTEM\ControlSetxxx\Services\<driver>\Parameters

Arguments:

RegistryPath - Registry path passed to DriverEntry

Returns:

NTSTATUS - SUCCESS if able to configure the framework

--*/

{
    PAGED_CODE();

    //
    // Initializing the unicode string, so that if it is not allocated it will not be deallocated too.
    //
    RtlInitUnicodeString(&g_RegistryPath, nullptr);

    g_RegistryPath.MaximumLength = RegistryPath->Length + sizeof(WCHAR);

    g_RegistryPath.Buffer = (PWCH)ExAllocatePool2(POOL_FLAG_PAGED, g_RegistryPath.MaximumLength, DRIVER_TAG);

    if (g_RegistryPath.Buffer == nullptr)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlAppendUnicodeToString(&g_RegistryPath, RegistryPath->Buffer);

    return STATUS_SUCCESS;
}

PAGED_CODE_SEG
NTSTATUS
Codec_EvtBusDeviceAdd(
    _In_    WDFDRIVER        Driver,
    _Inout_ PWDFDEVICE_INIT  DeviceInit
)
/*++

Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device. All the software resources
    should be allocated in this callback.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS                            status = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES               attributes;
    WDF_DEVICE_PNP_CAPABILITIES         pnpCaps;
    ACX_DEVICEINIT_CONFIG               devInitCfg;
    ACX_DEVICE_CONFIG                   devCfg;
    WDFDEVICE                           device = nullptr;
    PCODEC_DEVICE_CONTEXT               devCtx;
    WDF_PNPPOWER_EVENT_CALLBACKS        pnpPowerCallbacks;
    WDF_IO_QUEUE_CONFIG                 queueConfig;


    PAGED_CODE();

    UNREFERENCED_PARAMETER(Driver);

    //
    // The driver calls this DDI in its AddDevice callback before creating the PnP device.
    // ACX uses this call to add default/standard settings for the device to be created.
    //
    ACX_DEVICEINIT_CONFIG_INIT(&devInitCfg);
    RETURN_IF_FAILED(AcxDeviceInitInitialize(DeviceInit, &devInitCfg));

    //
    // Initialize the pnpPowerCallbacks structure.  Callback events for PNP
    // and Power are specified here.  If you don't supply any callbacks,
    // the Framework will take appropriate default actions based on whether
    // DeviceInit is initialized to be an FDO, a PDO or a filter device
    // object.
    //
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = Codec_EvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = Codec_EvtDeviceReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = Codec_EvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = Codec_EvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, CODEC_DEVICE_CONTEXT);
    attributes.EvtCleanupCallback = Codec_EvtDeviceContextCleanup;

    RETURN_NTSTATUS_IF_FAILED(WdfDeviceCreate(&DeviceInit, &attributes, &device));

    //
    // Init Codec's device context.
    //
    devCtx = GetCodecDeviceContext(device);
    ASSERT(devCtx != nullptr);

    // Initialize existing fields
    devCtx->Render = nullptr;
    devCtx->Capture = nullptr;
    devCtx->ExcludeD3Cold = WdfFalse;

    // Initialize new LAMAConnect fields
    devCtx->SharedBuffer = nullptr;
    devCtx->SharedMemoryHandle = NULL;
    devCtx->SharedMemoryBase = nullptr;
    devCtx->CompletionEventHandle = NULL;
    devCtx->LamaClientRegistered = FALSE;
    devCtx->LamaSampleRate = 0;
    devCtx->LamaBufferSizeFrames = 0;
    devCtx->LamaPluginChannelCount = 0;
    devCtx->LamaIoQueue = nullptr; // Will be set by WdfIoQueueCreate

    //
    // The driver calls this DDI in its AddDevice callback after creating the PnP
    // device. ACX uses this call to apply any post device settings.
    //
    ACX_DEVICE_CONFIG_INIT(&devCfg);
    RETURN_NTSTATUS_IF_FAILED(AcxDeviceInitialize(device, &devCfg));

    // Create LAMAConnect Device Interface
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_LAMACONNECT,
        NULL // ReferenceString
    );
    if (!NT_SUCCESS(status)) {
        // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: WdfDeviceCreateDeviceInterface failed %!STATUS!\n", status));
        return status;
    }

    // Create Symbolic Link for LAMAConnect Device Interface
    status = WdfDeviceCreateSymbolicLink(
        device,
        &symbolicLinkName
    );
    if (!NT_SUCCESS(status)) {
        // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: WdfDeviceCreateSymbolicLink failed %!STATUS!\n", status));
        return status;
    }

    // Configure I/O Queue for LAMAConnect IOCTLs
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = Codec_EvtIoDeviceControl;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &devCtx->LamaIoQueue // Store the queue handle in our device context
    );
    if (!NT_SUCCESS(status)) {
        // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: WdfIoQueueCreate failed %!STATUS!\n", status));
        return status;
    }

    //
    // Tell the framework to set the SurpriseRemovalOK in the DeviceCaps so
    // that you don't get the popup in usermode (on Win2K) when you surprise
    // remove the device.
    //
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.SurpriseRemovalOK = WdfTrue;
    WdfDeviceSetPnpCapabilities(device, &pnpCaps);

    //
    // Create a render circuit and capture circuit and add them to the current
    // device context. These circuits will be added to the device when the
    // prepare hardware callback is called. 
    //
    RETURN_NTSTATUS_IF_FAILED(CodecR_AddStaticRender(device, &CODEC_RENDER_COMPONENT_GUID, &renderCircuitName));

    RETURN_NTSTATUS_IF_FAILED(CodecC_AddStaticCapture(device, &CODEC_CAPTURE_COMPONENT_GUID, &MIC_CUSTOM_NAME, &captureCircuitName));

    return status;
}

PAGED_CODE_SEG
NTSTATUS
Codec_EvtDevicePrepareHardware(
    _In_ WDFDEVICE      Device,
    _In_ WDFCMRESLIST   ResourceList,
    _In_ WDFCMRESLIST   ResourceListTranslated
)
/*++

Routine Description:

    In this callback, the driver does whatever is necessary to make the
    hardware ready to use.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

--*/
{
    NTSTATUS                status = STATUS_SUCCESS;
    PCODEC_DEVICE_CONTEXT   devCtx;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    devCtx = GetCodecDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    // NOTE: Download firmware here.

    // NOTE: Register streaming h/w resources here.

    //
    // Set power policy data.
    //
    RETURN_NTSTATUS_IF_FAILED(Codec_SetPowerPolicy(Device));

    //
    // Setting up the data saving (CSaveData) and wave file reader (CWaveReader)
    // utility classes which will be used by the virtual streaming engine.
    //
    RETURN_NTSTATUS_IF_FAILED(CSaveData::SetDeviceObject(WdfDeviceWdmGetDeviceObject(Device)));

    RETURN_NTSTATUS_IF_FAILED(CSaveData::InitializeWorkItems(WdfDeviceWdmGetDeviceObject(Device)));

    RETURN_NTSTATUS_IF_FAILED(CWaveReader::InitializeWorkItems(WdfDeviceWdmGetDeviceObject(Device)));

    //
    // The driver uses this DDI to associate a circuit to a device. After
    // this call the circuit is not visible until the device goes in D0.
    // For a real driver there should be a check here to make sure the
    // circuit has not been added already (there could be a situation where
    // prepareHardware is called multiple times and releaseHardware is only
    // called once). 
    //

    ASSERT(devCtx->Render);
    RETURN_NTSTATUS_IF_FAILED(AcxDeviceAddCircuit(Device, devCtx->Render));

    ASSERT(devCtx->Capture);
    RETURN_NTSTATUS_IF_FAILED(AcxDeviceAddCircuit(Device, devCtx->Capture));

    return status;
}

PAGED_CODE_SEG
NTSTATUS
Codec_EvtDeviceReleaseHardware(
    _In_ WDFDEVICE      Device,
    _In_ WDFCMRESLIST   ResourceListTranslated
)
/*++

Routine Description:

    In this callback, the driver releases the h/w resources allocated in the
    prepare h/w callback.

Arguments:

    Device - handle to a device

Return Value:

    NT status value

--*/
{
    NTSTATUS                status;
    PCODEC_DEVICE_CONTEXT   devCtx;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    devCtx = GetCodecDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    //
    // The driver uses this DDI to delete a circuit from the current device. 
    //
    RETURN_NTSTATUS_IF_FAILED(AcxDeviceRemoveCircuit(Device, devCtx->Render));
    RETURN_NTSTATUS_IF_FAILED(AcxDeviceRemoveCircuit(Device, devCtx->Capture));

    // NOTE: Release streaming h/w resources here.

    CSaveData::DestroyWorkItems();
    CWaveReader::DestroyWorkItems();

    status = STATUS_SUCCESS;

    return status;
}

PAGED_CODE_SEG
NTSTATUS
Codec_EvtDeviceD0Entry(
    _In_  WDFDEVICE              Device,
    _In_  WDF_POWER_DEVICE_STATE PreviousState
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);

    PAGED_CODE();

    return STATUS_SUCCESS;
}

NTSTATUS
Codec_EvtDeviceD0Exit(
    _In_  WDFDEVICE              Device,
    _In_  WDF_POWER_DEVICE_STATE TargetState
)
{
    NTSTATUS        status = STATUS_SUCCESS;
    POWER_ACTION    powerAction;

    PAGED_CODE();

    powerAction = WdfDeviceGetSystemPowerAction(Device);

    // 
    // Update the power policy D3-cold info for Connected Standby.
    //
    if (TargetState == WdfPowerDeviceD3 && powerAction == PowerActionNone)
    {
        PCODEC_DEVICE_CONTEXT   devCtx;
        WDF_TRI_STATE           excludeD3Cold = WdfTrue;
        ACX_DX_EXIT_LATENCY     latency;

        devCtx = GetCodecDeviceContext(Device);
        ASSERT(devCtx != nullptr);

        //
        // Get the current exit latency.
        //
        latency = AcxDeviceGetCurrentDxExitLatency(Device,
            WdfDeviceGetSystemPowerAction(Device),
            TargetState);

        //
        // If the current exit latency for the ACX device is responsive
        // (not instant or fast) then D3-cold does not need to be excluded.
        // Otherwise, D3-cold should be excluded because if the hardware
        // goes into this state it will take too long to go back into D0
        // and respond. 
        //
        if (latency == AcxDxExitLatencyResponsive)
        {
            excludeD3Cold = WdfFalse;
        }

        if (devCtx->ExcludeD3Cold != excludeD3Cold)
        {
            devCtx->ExcludeD3Cold = excludeD3Cold;

            RETURN_NTSTATUS_IF_FAILED(Codec_SetPowerPolicy(Device));
        }
    }

    return status;
}

PAGED_CODE_SEG
NTSTATUS
Codec_SetPowerPolicy(
    _In_ WDFDEVICE      Device
)
{
    NTSTATUS                status = STATUS_SUCCESS;
    PCODEC_DEVICE_CONTEXT   devCtx;

    PAGED_CODE();

    devCtx = GetCodecDeviceContext(Device);
    ASSERT(devCtx != nullptr);

    //
    // Init the idle policy structure.
    //
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0);
    idleSettings.IdleTimeout = IDLE_POWER_TIMEOUT;
    idleSettings.IdleTimeoutType = SystemManagedIdleTimeoutWithHint;
    idleSettings.ExcludeD3Cold = devCtx->ExcludeD3Cold;

    RETURN_NTSTATUS_IF_FAILED(WdfDeviceAssignS0IdleSettings(Device, &idleSettings));

    return status;
}

VOID
Codec_EvtDeviceContextCleanup(
    _In_ WDFOBJECT      WdfDevice
)
/*++

Routine Description:

    In this callback, it cleans up device context.
    This is also where we would clean up LAMAConnect resources like unmapping shared memory
    and closing handles if they were created/opened.

Arguments:

    WdfDevice - WDF device object

Return Value:

    nullptr

--*/
{
    WDFDEVICE               device;
    PCODEC_DEVICE_CONTEXT   devCtx;

    PAGED_CODE(); 

    device = (WDFDEVICE)WdfDevice;
    devCtx = GetCodecDeviceContext(device);
    ASSERT(devCtx != nullptr);

    // Existing cleanup
    if (devCtx->Capture)
    {
        CodecC_CircuitCleanup(devCtx->Capture);
        devCtx->Capture = nullptr; 
    }

    // LAMAConnect specific cleanup
    // This check ensures cleanup happens if resources were allocated,
    // regardless of whether UNREGISTER was called.
    if (devCtx->SharedMemoryBase) 
    {
        NTSTATUS unmapStatus = ZwUnmapViewOfSection(NtCurrentProcess(), devCtx->SharedMemoryBase);
        if (!NT_SUCCESS(unmapStatus)) {
            // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwUnmapViewOfSection failed in cleanup %!STATUS!\n", unmapStatus));
        }
        devCtx->SharedMemoryBase = nullptr;
        devCtx->SharedBuffer = nullptr; // Important: SharedBuffer points into SharedMemoryBase
    }
    if (devCtx->SharedMemoryHandle)
    {
        ZwClose(devCtx->SharedMemoryHandle);
        devCtx->SharedMemoryHandle = NULL;
    }
    if (devCtx->CompletionEventHandle)
    {
        ZwClose(devCtx->CompletionEventHandle);
        devCtx->CompletionEventHandle = NULL;
    }
    devCtx->LamaClientRegistered = FALSE; // Ensure flag is reset

    // Note: LamaIoQueue is a WDF object and will be parented to the WDFDEVICE,
    // so it should be automatically cleaned up by WDF unless specific non-default parentage was set.
}

// Definition of the IOCTL handler function
VOID Codec_EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    PAGED_CODE(); 

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST; // Default status
    ULONG_PTR information = 0;                       // Default information
    PCODEC_DEVICE_CONTEXT devCtx = GetCodecDeviceContext(WdfIoQueueGetDevice(Queue));

    // OutputBufferLength is used by GET_STATUS
    // InputBufferLength is used by SET_FORMAT and TRIGGER_PROCESSING

    switch (IoControlCode)
    {
        case IOCTL_LAMA_CONNECT_REGISTER:
        {
            UNREFERENCED_PARAMETER(InputBufferLength); 
            UNREFERENCED_PARAMETER(OutputBufferLength);
            // 1. Check if already registered
            if (devCtx->LamaClientRegistered)
            {
                status = STATUS_DEVICE_ALREADY_ATTACHED;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: Client already registered.\n"));
                break; 
            }

            // 3. Define maxFramesForSharedBuffer
            UINT32 maxFramesForSharedBuffer = LAMA_CONNECT_MAX_BUFFER_SIZE;

            // 4. Calculate pluginToDriverDataSize
            SIZE_T pluginToDriverDataSize = (SIZE_T)LAMA_CONNECT_MAX_CHANNELS * maxFramesForSharedBuffer * sizeof(float);

            // 5. Calculate driverToPluginDataSize
            SIZE_T driverToPluginDataSize = pluginToDriverDataSize;

            // 6. Calculate totalDataSize
            SIZE_T totalDataSize = pluginToDriverDataSize + driverToPluginDataSize;

            // 7. Calculate requiredSharedMemSize
            SIZE_T requiredSharedMemSize = sizeof(LAMA_CONNECT_SHARED_BUFFER) + totalDataSize;

            // 8. Align this size to page boundaries
            SIZE_T alignedSharedMemSize = LAMA_ALIGN_TO_PAGE(requiredSharedMemSize);
            // Ensure LAMA_ALIGN_TO_PAGE is available or define it: ( (size) + PAGE_SIZE - 1 ) & ~(PAGE_SIZE - 1)

            // 9. Create Shared Memory Section
            UNICODE_STRING sectionName;
            // Using the macro from LAMAConnectShared.h (assuming it's defined as a L"" string)
            RtlInitUnicodeString(&sectionName, LAMA_CONNECT_SHARED_MEMORY_NAME L"0"); // Append instance "0"

            OBJECT_ATTRIBUTES objAttributes;
            InitializeObjectAttributes(&objAttributes, &sectionName, OBJ_KERNEL_HANDLE | OBJ_OPENIF | OBJ_CASE_INSENSITIVE, NULL, NULL);
            
            LARGE_INTEGER sectionSize;
            sectionSize.QuadPart = alignedSharedMemSize;

            status = ZwCreateSection(&devCtx->SharedMemoryHandle, SECTION_ALL_ACCESS, &objAttributes, &sectionSize, PAGE_READWRITE, SEC_COMMIT, NULL);
            if (!NT_SUCCESS(status))
            {
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwCreateSection failed %!STATUS!\n", status));
                devCtx->SharedMemoryHandle = NULL; // Ensure handle is NULL on failure
                break;
            }

            // 10. Map Shared Memory View
            SIZE_T viewSize = alignedSharedMemSize; // For ZwMapViewOfSection, this is both input and output for size
            status = ZwMapViewOfSection(devCtx->SharedMemoryHandle, NtCurrentProcess(), &devCtx->SharedMemoryBase, 0, 
                                        alignedSharedMemSize, // CommitSize, use full for non-large page backed sections
                                        NULL, &viewSize, ViewShare, 0, PAGE_READWRITE);
            if (!NT_SUCCESS(status))
            {
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwMapViewOfSection failed %!STATUS!\n", status));
                ZwClose(devCtx->SharedMemoryHandle);
                devCtx->SharedMemoryHandle = NULL;
                devCtx->SharedMemoryBase = NULL; // Ensure base is NULL on failure
                break;
            }
            devCtx->SharedBuffer = (PLAMA_CONNECT_SHARED_BUFFER)devCtx->SharedMemoryBase;

            // 11. Initialize LAMA_CONNECT_SHARED_BUFFER
            RtlZeroMemory(devCtx->SharedBuffer, alignedSharedMemSize);
            devCtx->SharedBuffer->Magic = LAMA_SHARED_BUFFER_MAGIC;
            devCtx->SharedBuffer->Version = LAMA_SHARED_BUFFER_VERSION;
            devCtx->SharedBuffer->StructSize = sizeof(LAMA_CONNECT_SHARED_BUFFER);
            devCtx->SharedBuffer->ChannelCount = LAMA_CONNECT_MAX_CHANNELS; // Driver fixed to this
            devCtx->SharedBuffer->SampleRate = 0; // To be set by IOCTL_LAMA_CONNECT_SET_FORMAT
            devCtx->SharedBuffer->BufferSize = 0; // To be set by IOCTL_LAMA_CONNECT_SET_FORMAT
            devCtx->SharedBuffer->BytesPerFrame = LAMA_CONNECT_MAX_CHANNELS * sizeof(float);
            devCtx->SharedBuffer->BufferState = BUFFER_STATE_EMPTY;
            devCtx->SharedBuffer->IsActive = FALSE; // Initialize IsActive to FALSE
            devCtx->SharedBuffer->PluginToDriverBufferOffset = sizeof(LAMA_CONNECT_SHARED_BUFFER);
            devCtx->SharedBuffer->DriverToPluginBufferOffset = sizeof(LAMA_CONNECT_SHARED_BUFFER) + (UINT32)pluginToDriverDataSize;
            devCtx->SharedBuffer->PluginToDriverBufferSize = (UINT32)pluginToDriverDataSize;
            devCtx->SharedBuffer->DriverToPluginBufferSize = (UINT32)driverToPluginDataSize;
            // devCtx->SharedBuffer->ValidationChecksum = LAMACalculateSimpleChecksum((UINT8*)devCtx->SharedBuffer, sizeof(LAMA_CONNECT_SHARED_BUFFER) - sizeof(UINT32));


            // 12. Create Completion Event
            UNICODE_STRING eventName;
            RtlInitUnicodeString(&eventName, LAMA_CONNECT_COMPLETION_EVENT_NAME L"0"); // Append instance "0"

            InitializeObjectAttributes(&objAttributes, &eventName, OBJ_KERNEL_HANDLE | OBJ_OPENIF | OBJ_CASE_INSENSITIVE, NULL, NULL);
            status = ZwCreateEvent(&devCtx->CompletionEventHandle, EVENT_ALL_ACCESS, &objAttributes, NotificationEvent, FALSE); // NotificationEvent, InitialState = FALSE
            if (!NT_SUCCESS(status))
            {
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwCreateEvent failed %!STATUS!\n", status));
                ZwUnmapViewOfSection(NtCurrentProcess(), devCtx->SharedMemoryBase);
                devCtx->SharedMemoryBase = NULL;
                ZwClose(devCtx->SharedMemoryHandle);
                devCtx->SharedMemoryHandle = NULL;
                devCtx->CompletionEventHandle = NULL; // Ensure handle is NULL
                break;
            }

            // 13. Set client registered flag
            devCtx->LamaClientRegistered = TRUE;
            status = STATUS_SUCCESS;
            information = 0; // No information to return for this IOCTL on success.
            // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "LAMA: Client registered successfully.\n"));
        }
        break; 

        case IOCTL_LAMA_CONNECT_UNREGISTER:
        {
            UNREFERENCED_PARAMETER(InputBufferLength); 
            UNREFERENCED_PARAMETER(OutputBufferLength);
            // 2. Retrieve device context - already done above
            // 3. Check LamaClientRegistered
            if (!devCtx->LamaClientRegistered)
            {
                status = STATUS_DEVICE_NOT_CONNECTED;
                information = 0;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: No client to unregister.\n"));
                break;
            }

            // 4. Set IsActive to FALSE
            if (devCtx->SharedBuffer)
            {
                devCtx->SharedBuffer->IsActive = FALSE; // Ensure IsActive is FALSE on unregister
            }

            // 5. Unmap SharedMemoryBase
            if (devCtx->SharedMemoryBase)
            {
                NTSTATUS unmapStatus = ZwUnmapViewOfSection(NtCurrentProcess(), devCtx->SharedMemoryBase);
                if (!NT_SUCCESS(unmapStatus))
                {
                    // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwUnmapViewOfSection failed %!STATUS!\n", unmapStatus));
                    // Log error but continue cleanup
                }
                devCtx->SharedMemoryBase = NULL;
                devCtx->SharedBuffer = NULL; // SharedBuffer pointed into SharedMemoryBase
            }

            // 6. Close SharedMemoryHandle
            if (devCtx->SharedMemoryHandle)
            {
                NTSTATUS closeStatus = ZwClose(devCtx->SharedMemoryHandle);
                if (!NT_SUCCESS(closeStatus))
                {
                    // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwClose SharedMemoryHandle failed %!STATUS!\n", closeStatus));
                    // Log error but continue cleanup
                }
                devCtx->SharedMemoryHandle = NULL;
            }

            // 7. Close CompletionEventHandle
            if (devCtx->CompletionEventHandle)
            {
                NTSTATUS closeStatus = ZwClose(devCtx->CompletionEventHandle);
                if (!NT_SUCCESS(closeStatus))
                {
                    // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: ZwClose CompletionEventHandle failed %!STATUS!\n", closeStatus));
                    // Log error but continue cleanup
                }
                devCtx->CompletionEventHandle = NULL;
            }

            // 8. Reset LamaClientRegistered flag
            devCtx->LamaClientRegistered = FALSE;
            // 9. Reset LamaSampleRate
            devCtx->LamaSampleRate = 0;
            // 10. Reset LamaBufferSizeFrames
            devCtx->LamaBufferSizeFrames = 0;
            // 11. Reset LamaPluginChannelCount
            devCtx->LamaPluginChannelCount = 0;

            status = STATUS_SUCCESS;
            information = 0;
            // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "LAMA: Client unregistered successfully.\n"));
        }
        break;

        case IOCTL_LAMA_CONNECT_SET_FORMAT:
        {
            PLAMA_CONNECT_FORMAT clientFormat = NULL;
            size_t retrievedBufferLength = 0; 

            UNREFERENCED_PARAMETER(OutputBufferLength);

            if (!devCtx->LamaClientRegistered)
            {
                status = STATUS_DEVICE_NOT_CONNECTED;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: SET_FORMAT - No client registered.\n"));
                break;
            }
            
            if (devCtx->SharedBuffer == NULL) // Should not happen if LamaClientRegistered is TRUE
            {
                status = STATUS_INVALID_DEVICE_STATE;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: SET_FORMAT - SharedBuffer is NULL despite client registration.\n"));
                break;
            }

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(LAMA_CONNECT_FORMAT), (PVOID*)&clientFormat, &retrievedBufferLength);
            if (!NT_SUCCESS(status))
            {
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: SET_FORMAT - WdfRequestRetrieveInputBuffer failed %!STATUS!\n", status));
                break; 
            }

            if (retrievedBufferLength < sizeof(LAMA_CONNECT_FORMAT))
            {
                status = STATUS_BUFFER_TOO_SMALL;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: SET_FORMAT - Input buffer too small (%Iu bytes).\n", retrievedBufferLength));
                break;
            }

            if (!LAMAIsValidFormat(clientFormat))
            {
                status = STATUS_INVALID_PARAMETER;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: SET_FORMAT - LAMAIsValidFormat failed.\n"));
                break;
            }

            devCtx->LamaSampleRate = clientFormat->SampleRate;
            devCtx->LamaBufferSizeFrames = clientFormat->BufferSize;
            devCtx->LamaPluginChannelCount = clientFormat->ChannelCount; 

            devCtx->SharedBuffer->SampleRate = clientFormat->SampleRate;
            devCtx->SharedBuffer->BufferSize = clientFormat->BufferSize; 
            devCtx->SharedBuffer->IsActive = TRUE; // Set IsActive to TRUE after format is set
            
            status = STATUS_SUCCESS;
            information = 0; 
        }
        break;

        case IOCTL_LAMA_CONNECT_TRIGGER_PROCESSING:
        {
            PUINT32 pFramesInThisBlock = NULL;
            size_t retrievedInputBufferLength = 0; 
            UNREFERENCED_PARAMETER(OutputBufferLength);

            if (!devCtx->LamaClientRegistered) { status = STATUS_DEVICE_NOT_CONNECTED; break; }
            if (!devCtx->SharedBuffer) { status = STATUS_INVALID_DEVICE_STATE; break; }
            if (devCtx->LamaSampleRate == 0 || devCtx->LamaBufferSizeFrames == 0 || !devCtx->SharedBuffer->IsActive) { // Added IsActive check
                status = STATUS_DEVICE_NOT_READY; 
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: TRIGGER - Device not ready (SR:%u, BS:%u, Active:%d)\n", 
                //    devCtx->LamaSampleRate, devCtx->LamaBufferSizeFrames, devCtx->SharedBuffer->IsActive));
                break; 
            }

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(UINT32), (PVOID*)&pFramesInThisBlock, &retrievedInputBufferLength);
            if (!NT_SUCCESS(status)) { /* KdPrint for error */ break; }
            if (retrievedInputBufferLength < sizeof(UINT32)) { status = STATUS_BUFFER_TOO_SMALL; break; }
            
            UINT32 framesInThisBlock = *pFramesInThisBlock;

            if (framesInThisBlock == 0 || framesInThisBlock > devCtx->LamaBufferSizeFrames) {
                status = STATUS_INVALID_PARAMETER;
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: TRIGGER - Invalid framesInThisBlock: %u (Max: %u)\n", framesInThisBlock, devCtx->LamaBufferSizeFrames));
                break;
            }

            if (devCtx->SharedBuffer->BufferState == BUFFER_STATE_PROCESSING) {
                status = STATUS_DEVICE_BUSY; 
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "LAMA: TRIGGER - Buffer already in processing state.\n"));
                break; 
            }
            devCtx->SharedBuffer->BufferState = BUFFER_STATE_PROCESSING;

            PBYTE sharedMemoryBaseBytes = (PBYTE)devCtx->SharedMemoryBase;
            // float* pluginToDriverBuffer = (float*)(sharedMemoryBaseBytes + devCtx->SharedBuffer->PluginToDriverBufferOffset); // Input buffer not used for silence
            float* driverToPluginBuffer = (float*)(sharedMemoryBaseBytes + devCtx->SharedBuffer->DriverToPluginBufferOffset);

            ULONG bytesToZero = devCtx->LamaPluginChannelCount * framesInThisBlock * sizeof(float);
            
            // Bounds check for bytesToZero
            if (bytesToZero > devCtx->SharedBuffer->DriverToPluginBufferSize) {
                 // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "LAMA: TRIGGER - bytesToZero %u exceeds DriverToPluginBufferSize %u. Capping.\n", 
                 //    bytesToZero, devCtx->SharedBuffer->DriverToPluginBufferSize));
                 bytesToZero = devCtx->SharedBuffer->DriverToPluginBufferSize;
                 // Or alternatively, return an error:
                 // devCtx->SharedBuffer->BufferState = BUFFER_STATE_ERROR; 
                 // status = STATUS_BUFFER_OVERFLOW; 
                 // break;
            }
            
            // Fill the driver-to-plugin buffer with silence
            if (bytesToZero > 0) // Ensure bytesToZero is positive before calling RtlZeroMemory
            {
                RtlZeroMemory(driverToPluginBuffer, bytesToZero);
            }
            
            devCtx->SharedBuffer->ProcessedFrames += framesInThisBlock;
            devCtx->SharedBuffer->BufferState = BUFFER_STATE_EMPTY; 

            NTSTATUS eventStatus = ZwSetEvent(devCtx->CompletionEventHandle, NULL);
            if (!NT_SUCCESS(eventStatus)) {
                // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "LAMA: TRIGGER - ZwSetEvent failed %!STATUS!\n", eventStatus));
            }
            
            status = STATUS_SUCCESS;
            information = 0;
        }
        break;

        case IOCTL_LAMA_CONNECT_GET_STATUS:
        {
            UNREFERENCED_PARAMETER(InputBufferLength);
            PLAMA_CONNECT_SHARED_BUFFER outputBuffer = NULL;
            size_t retrievedOutputBufferLength = 0;

            if (!devCtx->LamaClientRegistered) { status = STATUS_DEVICE_NOT_CONNECTED; break; }
            if (!devCtx->SharedBuffer) { status = STATUS_INVALID_DEVICE_STATE; break; }

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(LAMA_CONNECT_SHARED_BUFFER), (PVOID*)&outputBuffer, &retrievedOutputBufferLength);
            if (!NT_SUCCESS(status)) {
                // KdPrintEx for error, status is already set
                break;
            }
            // WdfRequestRetrieveOutputBuffer with MinimumRequiredLength guarantees this, but defensive check is fine.
            if (retrievedOutputBufferLength < sizeof(LAMA_CONNECT_SHARED_BUFFER)) { 
                status = STATUS_BUFFER_TOO_SMALL; 
                break; 
            }

            RtlCopyMemory(outputBuffer, devCtx->SharedBuffer, sizeof(LAMA_CONNECT_SHARED_BUFFER));
            status = STATUS_SUCCESS;
            information = sizeof(LAMA_CONNECT_SHARED_BUFFER);
        }
        break;

        case IOCTL_LAMA_CONNECT_RESET_STATS:
        {
            UNREFERENCED_PARAMETER(InputBufferLength);
            UNREFERENCED_PARAMETER(OutputBufferLength);

            if (!devCtx->LamaClientRegistered) { status = STATUS_DEVICE_NOT_CONNECTED; break; }
            if (!devCtx->SharedBuffer) { status = STATUS_INVALID_DEVICE_STATE; break; }

            devCtx->SharedBuffer->ProcessedFrames = 0;
            devCtx->SharedBuffer->ErrorCount = 0;
            devCtx->SharedBuffer->OverrunCount = 0;
            devCtx->SharedBuffer->UnderrunCount = 0;
            devCtx->SharedBuffer->AverageProcessingTimeUs = 0;
            devCtx->SharedBuffer->PeakProcessingTimeUs = 0;
            devCtx->SharedBuffer->LastProcessingTimeUs = 0; 
            
            status = STATUS_SUCCESS;
            information = 0;
        }
        break;

        default:
            UNREFERENCED_PARAMETER(InputBufferLength); 
            UNREFERENCED_PARAMETER(OutputBufferLength);
            // KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "LAMA: Codec_EvtIoDeviceControl received unknown IOCTL: 0x%X\n", IoControlCode));
            status = STATUS_INVALID_DEVICE_REQUEST;
            information = 0;
            break;
    }

    WdfRequestCompleteWithInformation(Request, status, information);
}

[end of audio/Lama/ACX/DriverV2/AudioCodec/Driver/Device.cpp]
