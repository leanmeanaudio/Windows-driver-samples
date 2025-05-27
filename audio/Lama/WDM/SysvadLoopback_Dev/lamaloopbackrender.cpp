// Include our audio driver compatibility header first
#include "audio_compat.h"

// Compatibility fix for portcls.h must be first include
#include "portcls_compat.h"

// Include our direct fix header at the very beginning
#include "direct_fix.h"
#include "waveformat_fix.h"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility

// Include our unified compatibility header first
#include "sysvad_compat.h"

// Then include standard Windows headers
#include <initguid.h> // For DEFINE_GUID
#include <wdm.h>
#include <ks.h>
#include <ksmedia.h>
#include <portcls.h>   // For IPortClsSubdeviceEx
#include <Ntstrsafe.h> // For RtlStringCchPrintfW

#include "lamaloopbackcommon.h"
#include "lamaloopbackrender.h" // Includes CMiniportWaveRTLamaLoopbackStream and LamaRenderPinWrite forward decl
#include "hdmitopo.h"     // For CMiniportWaveRTLamaLoopbackStream class definition
#include "minipairs.h"  // For LamaLoopbackMiniportPairs and LAMA_LOOPBACK_MINIPAIR_COUNT

// If LAMA_POOL_TAG is not defined elsewhere (it should be in common.h or resource.h)
#ifndef LAMA_POOL_TAG
#define LAMA_POOL_TAG 'RLaL' 
#endif

// ADAPTER_DEVICE_EXTENSION structure (already defined)

#pragma code_seg("INIT") 

//=============================================================================
// DriverEntry
//=============================================================================
extern "C" NTSTATUS
DriverEntry
(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    PAGED_CODE();
    DPF_ENTER(("[DriverEntry] RegistryPath: %wZ", RegistryPath));
    NTSTATUS ntStatus = STATUS_SUCCESS;

    ntStatus = InitializeAllSharedLoopbackBuffers();
    if (!NT_SUCCESS(ntStatus))
    {
        DPF(DPF_LEVEL_ERROR, ("Failed to initialize all shared loopback buffers, 0x%x", ntStatus));
    }

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->DriverExtension->AddDevice = AddDevice; 
    DriverObject->MajorFunction[IRP_MJ_CREATE]          = LamaCreateDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]           = LamaCloseDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = LamaIoControlDispatch;
    DriverObject->MajorFunction[IRP_MJ_READ]            = LamaReadDispatch; 
    DriverObject->MajorFunction[IRP_MJ_WRITE]           = LamaWriteDispatch; 
    DriverObject->MajorFunction[IRP_MJ_PNP]             = LamaPnpDispatch;
    DriverObject->MajorFunction[IRP_MJ_POWER]           = LamaPowerDispatch;
    
    ntStatus = PcInitializeAdapterDriver(DriverObject, RegistryPath, NULL);
    if (!NT_SUCCESS(ntStatus)) { 
        DPF(DPF_LEVEL_ERROR, ("PcInitializeAdapterDriver failed, 0x%x", ntStatus));
        FreeAllSharedLoopbackBuffers(); 
    }
    DPF_LEAVE(("[DriverEntry] ntStatus=0x%08x", ntStatus));
    return ntStatus;
}

#pragma code_seg("PAGE")

//=============================================================================
// DriverUnload
//=============================================================================
VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{ 
    PAGED_CODE(); 
    DPF_ENTER(("[DriverUnload]")); 
    PcUninitializeAdapterDriver(DriverObject); 
    FreeAllSharedLoopbackBuffers(); 
    DPF_LEAVE(("[DriverUnload]")); 
}

//=============================================================================
// AddDevice (Adapter's AddDevice)
//=============================================================================
NTSTATUS AddDevice(_In_ PDRIVER_OBJECT DriverObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    PAGED_CODE(); DPF_ENTER(("[AddDevice] PDO=0x%p", PhysicalDeviceObject));
    NTSTATUS ntStatus = STATUS_SUCCESS; PDEVICE_OBJECT functionalDeviceObject = NULL; PADAPTER_DEVICE_EXTENSION pAdapterExt = NULL;
    WCHAR deviceNameBuffer[64]; UNICODE_STRING deviceNameUnicodeString;
    ntStatus = IoCreateDevice(DriverObject, sizeof(ADAPTER_DEVICE_EXTENSION), NULL, FILE_DEVICE_SOUND, FILE_DEVICE_SECURE_OPEN, FALSE, &functionalDeviceObject);
    if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("IoCreateDevice failed for Adapter FDO, 0x%x", ntStatus)); goto Exit; }
    pAdapterExt = (PADAPTER_DEVICE_EXTENSION)functionalDeviceObject->DeviceExtension; RtlZeroMemory(pAdapterExt, sizeof(ADAPTER_DEVICE_EXTENSION)); 
    pAdapterExt->PhysicalDeviceObject = PhysicalDeviceObject; pAdapterExt->FunctionalDeviceObject = functionalDeviceObject;
    pAdapterExt->NextLowerDriver = IoAttachDeviceToDeviceStack(functionalDeviceObject, PhysicalDeviceObject);
    if (pAdapterExt->NextLowerDriver == NULL) { DPF(DPF_LEVEL_ERROR, ("IoAttachDeviceToDeviceStack failed for Adapter FDO")); ntStatus = STATUS_NO_SUCH_DEVICE; goto Exit; }
    if (LAMA_LOOPBACK_MINIPAIR_COUNT < 2) { DPF(DPF_LEVEL_ERROR, ("Insufficient miniport pair definitions. Need at least 2. Got %d.", LAMA_LOOPBACK_MINIPAIR_COUNT)); ntStatus = STATUS_DRIVER_INTERNAL_ERROR; goto Exit; }

    for (ULONG i = 0; i < LAMA_LOOPBACK_DEVICE_MAX_INSTANCES; ++i) {
        // Note: The instance 'i' from this loop is the intended instance index for the subdevices.
        // This index should be passed to the miniport Init/NewStream if PortCls doesn't provide another way.
        // For now, the miniport Init will try to parse it from the device object name.
        ntStatus = RtlStringCchPrintfW(deviceNameBuffer, SIZEOF_ARRAY(deviceNameBuffer), L"%s%d", LAMA_LOOPBACK_RENDER_BASENAME, i);
        if (NT_SUCCESS(ntStatus)) {
            RtlInitUnicodeString(&deviceNameUnicodeString, deviceNameBuffer); DPF(DPF_LEVEL_INFO, ("Registering Render Subdevice: %wZ (Instance %u)", &deviceNameUnicodeString, i));
            ntStatus = PcRegisterSubdevice(functionalDeviceObject, &deviceNameUnicodeString, LamaLoopbackMiniportPairs[0]->WaveFactory, LamaLoopbackMiniportPairs[0]->TopologyFactory, NULL, LamaLoopbackMiniportPairs[0]->TopologyFilterDescriptor, LamaLoopbackMiniportPairs[0]->WaveFilterDescriptor, NULL);
            if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("PcRegisterSubdevice for %wZ failed, 0x%x", &deviceNameUnicodeString, ntStatus)); }
        } else { DPF(DPF_LEVEL_ERROR, ("RtlStringCchPrintfW failed for Render device name (Instance %d), 0x%x", i, ntStatus)); }

        ntStatus = RtlStringCchPrintfW(deviceNameBuffer, SIZEOF_ARRAY(deviceNameBuffer), L"%s%d", LAMA_LOOPBACK_CAPTURE_BASENAME, i);
        if (NT_SUCCESS(ntStatus)) {
            RtlInitUnicodeString(&deviceNameUnicodeString, deviceNameBuffer); DPF(DPF_LEVEL_INFO, ("Registering Capture Subdevice: %wZ (Instance %u)", &deviceNameUnicodeString, i));
            ntStatus = PcRegisterSubdevice(functionalDeviceObject, &deviceNameUnicodeString, LamaLoopbackMiniportPairs[1]->WaveFactory, LamaLoopbackMiniportPairs[1]->TopologyFactory, NULL, LamaLoopbackMiniportPairs[1]->TopologyFilterDescriptor, LamaLoopbackMiniportPairs[1]->WaveFilterDescriptor, NULL);
            if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("PcRegisterSubdevice for %wZ failed, 0x%x", &deviceNameUnicodeString, ntStatus)); }
        } else { DPF(DPF_LEVEL_ERROR, ("RtlStringCchPrintfW failed for Capture device name (Instance %d), 0x%x", i, ntStatus)); }
    }
    ntStatus = STATUS_SUCCESS; functionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
Exit:
    if (!NT_SUCCESS(ntStatus)) { if (pAdapterExt && pAdapterExt->NextLowerDriver) { IoDetachDevice(pAdapterExt->NextLowerDriver); pAdapterExt->NextLowerDriver = NULL; } if (functionalDeviceObject) { IoDeleteDevice(functionalDeviceObject); } }
    DPF_LEAVE(("[AddDevice] ntStatus=0x%08x", ntStatus)); return ntStatus;
}

//=============================================================================
// Dispatch Routine Stubs (for the Adapter FDO) - Existing
//=============================================================================
NTSTATUS LamaCreateDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { PAGED_CODE(); UNREFERENCED_PARAMETER(DeviceObject); DPF_ENTER(("[LamaCreateDispatch]")); Irp->IoStatus.Status = STATUS_SUCCESS; Irp->IoStatus.Information = 0; IoCompleteRequest(Irp, IO_NO_INCREMENT); DPF_LEAVE(("[LamaCreateDispatch] STATUS_SUCCESS")); return STATUS_SUCCESS; }
NTSTATUS LamaCloseDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { PAGED_CODE(); UNREFERENCED_PARAMETER(DeviceObject); DPF_ENTER(("[LamaCloseDispatch]")); Irp->IoStatus.Status = STATUS_SUCCESS; Irp->IoStatus.Information = 0; IoCompleteRequest(Irp, IO_NO_INCREMENT); DPF_LEAVE(("[LamaCloseDispatch] STATUS_SUCCESS")); return STATUS_SUCCESS; }
NTSTATUS LamaIoControlDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { PAGED_CODE(); DPF_ENTER(("[LamaIoControlDispatch]")); NTSTATUS ntStatus = PcDispatchIrp(DeviceObject, Irp); DPF_LEAVE(("[LamaIoControlDispatch] ntStatus=0x%08x from PcDispatchIrp", ntStatus)); return ntStatus; }
NTSTATUS LamaReadDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { PAGED_CODE(); UNREFERENCED_PARAMETER(DeviceObject); DPF_ENTER(("[LamaReadDispatch]")); Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED; Irp->IoStatus.Information = 0; IoCompleteRequest(Irp, IO_NO_INCREMENT); DPF_LEAVE(("[LamaReadDispatch] STATUS_NOT_IMPLEMENTED")); return STATUS_NOT_IMPLEMENTED; }
NTSTATUS LamaWriteDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { PAGED_CODE(); UNREFERENCED_PARAMETER(DeviceObject); DPF_ENTER(("[LamaWriteDispatch]")); Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED; Irp->IoStatus.Information = 0; IoCompleteRequest(Irp, IO_NO_INCREMENT); DPF_LEAVE(("[LamaWriteDispatch] STATUS_NOT_IMPLEMENTED")); return STATUS_NOT_IMPLEMENTED; }
NTSTATUS LamaPnpDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { PAGED_CODE(); PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp); DPF_ENTER(("[LamaPnpDispatch] MinorFunction=0x%x for Adapter FDO 0x%p", irpSp->MinorFunction, DeviceObject)); PADAPTER_DEVICE_EXTENSION pAdapterExt = (PADAPTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension; NTSTATUS ntStatus; if (irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) { DPF(DPF_LEVEL_INFO, ("LamaPnpDispatch: IRP_MN_REMOVE_DEVICE for Adapter FDO 0x%p", DeviceObject)); if (pAdapterExt->NextLowerDriver) { IoDetachDevice(pAdapterExt->NextLowerDriver); pAdapterExt->NextLowerDriver = NULL; } IoDeleteDevice(DeviceObject); Irp->IoStatus.Status = STATUS_SUCCESS; Irp->IoStatus.Information = 0; IoCompleteRequest(Irp, IO_NO_INCREMENT); DPF_LEAVE(("[LamaPnpDispatch] IRP_MN_REMOVE_DEVICE handled for Adapter FDO, STATUS_SUCCESS")); return STATUS_SUCCESS; } ntStatus = PcDispatchPnpIrp(DeviceObject, Irp); DPF_LEAVE(("[LamaPnpDispatch] PcDispatchPnpIrp returned 0x%08x", ntStatus)); return ntStatus; }
NTSTATUS LamaPowerDispatch(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) { DPF_ENTER(("[LamaPowerDispatch] MinorFunction=0x%x for Adapter FDO 0x%p", IoGetCurrentIrpStackLocation(Irp)->MinorFunction, DeviceObject)); PADAPTER_DEVICE_EXTENSION pAdapterExt = (PADAPTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension; PoStartNextPowerIrp(Irp); IoSkipCurrentIrpStackLocation(Irp); NTSTATUS ntStatus = PoCallDriver(pAdapterExt->NextLowerDriver, Irp); DPF_LEAVE(("[LamaPowerDispatch] Forwarded IRP, ntStatus=0x%08x", ntStatus)); return ntStatus; }

//=============================================================================
// KSPROPERTY_LAMA_SAMPLE_RATE Handler - Existing
//=============================================================================
NTSTATUS LamaFilterPropertyHandler_SampleRate ( _In_ PIRP Irp, _In_ PKSPROPERTY Property, _Inout_ PVOID Data ) { /* ... existing full implementation ... */ 
    PAGED_CODE(); ASSERT(Irp); ASSERT(Property); ASSERT(Data); NTSTATUS ntStatus = STATUS_SUCCESS; PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp); DPF_ENTER(("[LamaFilterPropertyHandler_SampleRate] PropertyId=%u, Flags=0x%X", Property->Id, Property->Flags)); if (Property->Id != KSPROPERTY_LAMA_SAMPLE_RATE) { return STATUS_INVALID_PARAMETER; }
    if (Property->Flags & KSPROPERTY_TYPE_GET) { if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) { ntStatus = STATUS_BUFFER_TOO_SMALL; Irp->IoStatus.Information = 0; } else { *(PULONG)Data = g_CurrentGlobalSampleRate; ntStatus = STATUS_SUCCESS; Irp->IoStatus.Information = sizeof(ULONG); } }
    else if (Property->Flags & KSPROPERTY_TYPE_SET) { if (irpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) { ntStatus = STATUS_INVALID_PARAMETER; Irp->IoStatus.Information = 0; } else { ULONG newSampleRate = *(PULONG)Data; if (newSampleRate >= MIN_SAMPLE_RATE_PCM && newSampleRate <= MAX_SAMPLE_RATE_PCM) { g_CurrentGlobalSampleRate = newSampleRate; DPF(DPF_LEVEL_INFO, ("Set new global sample rate: %u Hz", g_CurrentGlobalSampleRate)); ntStatus = STATUS_SUCCESS; Irp->IoStatus.Information = sizeof(ULONG); } else { DPF(DPF_LEVEL_ERROR, ("Invalid sample rate set: %u Hz. Valid range %u-%u Hz.", newSampleRate, MIN_SAMPLE_RATE_PCM, MAX_SAMPLE_RATE_PCM)); ntStatus = STATUS_INVALID_PARAMETER; Irp->IoStatus.Information = 0; } } }
    else if (Property->Flags & KSPROPERTY_TYPE_BASICSUPPORT) { PKSPROPERTY_DESCRIPTION PropDesc = (PKSPROPERTY_DESCRIPTION)Data; if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(KSPROPERTY_DESCRIPTION)) { ntStatus = STATUS_BUFFER_TOO_SMALL; Irp->IoStatus.Information = 0; } else { PropDesc->AccessFlags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT; PropDesc->DescriptionSize = sizeof(KSPROPERTY_DESCRIPTION); PropDesc->PropTypeSet.Set = KSPROPSETID_LamaLoopback; PropDesc->PropTypeSet.Id = KSPROPERTY_LAMA_SAMPLE_RATE; PropDesc->PropTypeSet.Flags = 0; PropDesc->MembersListCount = 0; PropDesc->Reserved = 0; ntStatus = STATUS_SUCCESS; Irp->IoStatus.Information = sizeof(KSPROPERTY_DESCRIPTION); } }
    else { ntStatus = STATUS_INVALID_PARAMETER; Irp->IoStatus.Information = 0; } Irp->IoStatus.Status = ntStatus; DPF_LEAVE(("[LamaFilterPropertyHandler_SampleRate] ntStatus=0x%08x", ntStatus)); return ntStatus;
}

//=============================================================================
// Automation Table for KSPROPSETID_LamaLoopback - Existing
//=============================================================================
DEFINE_KSPROPERTY_TABLE(LamaLoopbackFilterPropertyTable) { DEFINE_KSPROPERTY_ITEM_LAMA_SAMPLE_RATE(LamaFilterPropertyHandler_SampleRate, LamaFilterPropertyHandler_SampleRate) };
DEFINE_KSPROPERTY_SET_TABLE(LamaLoopbackFilterAutomationTable) { DEFINE_KSPROPERTY_SET(&KSPROPSETID_LamaLoopback, SIZEOF_ARRAY(LamaLoopbackFilterPropertyTable), LamaLoopbackFilterPropertyTable, 0, NULL) };

//=============================================================================
// CMiniportTopologyLamaLoopbackRender - Existing
//=============================================================================
CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender(void) : CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL) { PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::CMiniportTopologyLamaLoopbackRender]")); }
CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender(void) { PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::~CMiniportTopologyLamaLoopbackRender]")); if (m_Port) { m_Port->Release(); m_Port = NULL; } if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; } }
NTSTATUS CMiniportTopologyLamaLoopbackRender::Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY Port_) { PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_); DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::Init]")); UNREFERENCED_PARAMETER(ResourceList); m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef(); m_Port = Port_; m_Port->AddRef(); return STATUS_SUCCESS; }
NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection
(
    _In_        ULONG           PinId,
    _In_        PKSDATARANGE    DataRange,          // Client's proposed data range
    _In_        PKSDATARANGE    MatchingDataRange,  // Pin's data range (PcmAudioDataRange)
    _In_        ULONG           OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID           ResultantFormat,    // Buffer for the resulting format
    _Out_       PULONG          ResultantFormatLength
)
{
    PAGED_CODE();
    DbgPrint("Entered CMiniportTopologyLamaLoopbackRender::DataRangeIntersection PinId=%u\n", PinId);
    UNREFERENCED_PARAMETER(MatchingDataRange);

    if (!DataRange || !ResultantFormatLength) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *ResultantFormatLength = 0; // Default to zero

    // Validate basic format type (Audio, PCM, WaveFormatEx)
    if (!IsEqualGUIDAligned(DataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned(DataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUIDAligned(DataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DbgPrint("DataRangeIntersection (Render): No match due to Major/Sub/Specifier GUIDs.\n");
        return STATUS_NO_MATCH;
    }

    // Get safe channel information using our helper function
    ULONG clientMinChannels = 1;
    ULONG clientMaxChannels = 2;
    GetSafeChannelInfo(DataRange, &clientMinChannels, &clientMaxChannels);
    ULONG resultChannels;

    if (clientMaxChannels == 0 || clientMaxChannels > MAX_CHANNELS_PCM) {
        clientMaxChannels = MAX_CHANNELS_PCM;
    }
   
    if (clientMinChannels > clientMaxChannels) {
        DbgPrint("DataRangeIntersection (Render): Client min channels %u > client max channels %u. No match.\n", 
            clientMinChannels, clientMaxChannels);
        return STATUS_NO_MATCH; 
    }
    
    if (clientMaxChannels >= MIN_CHANNELS_PCM && clientMaxChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMaxChannels;
    } else if (clientMinChannels >= MIN_CHANNELS_PCM && clientMinChannels <= MAX_CHANNELS_PCM) {
        resultChannels = clientMinChannels;
    } else {
        DbgPrint("DataRangeIntersection (Render): Client channel range [%u, %u] is outside device capabilities [%u, %u]. No match.\n",
            clientMinChannels, clientMaxChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM);
        return STATUS_NO_MATCH;
    }
   
    if (resultChannels < MIN_CHANNELS_PCM || resultChannels > MAX_CHANNELS_PCM) {
        DbgPrint("DataRangeIntersection (Render): Selected resultChannels %u is outside device capabilities [%u, %u]. No match.\n",
            resultChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM);
        return STATUS_NO_MATCH;
    }

    // Sample Rate and Bits Per Sample logic
    ULONG resultBitsPerSample = g_CurrentGlobalBitsPerSample;
    ULONG resultSampleRate = g_CurrentGlobalSampleRate;      

    // Get safe bit depth and sample rate information using our helper functions
    ULONG minBitsPerSample, maxBitsPerSample, minSampleRate, maxSampleRate;
    GetSafeBitDepthInfo(DataRange, &minBitsPerSample, &maxBitsPerSample);
    GetSafeSampleRateInfo(DataRange, &minSampleRate, &maxSampleRate);

    // Check compatibility with safe ranges
    if (resultBitsPerSample < minBitsPerSample || resultBitsPerSample > maxBitsPerSample) {
        DbgPrint("DataRangeIntersection (Render): Global BPS %u not in client range [%u, %u]\n", 
            resultBitsPerSample, minBitsPerSample, maxBitsPerSample);
        return STATUS_NO_MATCH;
    }

    if (resultSampleRate < minSampleRate || resultSampleRate > maxSampleRate) {
        DbgPrint("DataRangeIntersection (Render): Global SR %u not in client range [%u, %u]\n", 
            resultSampleRate, minSampleRate, maxSampleRate);
        return STATUS_NO_MATCH;
    }

    if (resultBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || resultBitsPerSample > MAX_BITS_PER_SAMPLE_PCM ||
        resultSampleRate < MIN_SAMPLE_RATE_PCM || resultSampleRate > MAX_SAMPLE_RATE_PCM) {
        DbgPrint("DataRangeIntersection (Render): Resulting format SR/BPS (%uHz, %ubit) outside device capabilities.\n",
            resultSampleRate, resultBitsPerSample);
        return STATUS_NO_MATCH;
    }

    if (!ResultantFormat) 
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
        return STATUS_BUFFER_OVERFLOW; 
    }

    if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))
    {
        DbgPrint("DataRangeIntersection: Output buffer too small. Needed %u, Got %u\n", 
            (ULONG)sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), OutputBufferLength);
        return STATUS_BUFFER_TOO_SMALL;
    }

    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pResFormatWfx = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat;
    
    pResFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pResFormatWfx->DataFormat.Flags = 0;
    pResFormatWfx->DataFormat.SampleSize = (resultBitsPerSample / 8) * resultChannels; 
    pResFormatWfx->DataFormat.Reserved = 0;
    pResFormatWfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    pResFormatWfx->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    pResFormatWfx->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    WAVEFORMATEXTENSIBLE *pWfx = &pResFormatWfx->WaveFormatExt;
    pWfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    pWfx->Format.nChannels = (WORD)resultChannels;
    pWfx->Format.nSamplesPerSec = resultSampleRate;
    pWfx->Format.wBitsPerSample = (WORD)resultBitsPerSample;
    pWfx->Format.nBlockAlign = (WORD)((pWfx->Format.nChannels * pWfx->Format.wBitsPerSample) / 8);
    pWfx->Format.nAvgBytesPerSec = pWfx->Format.nSamplesPerSec * pWfx->Format.nBlockAlign;
    pWfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    
    pWfx->Samples.wValidBitsPerSample = pWfx->Format.wBitsPerSample;
    
    if (pWfx->Format.nChannels == 1) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_MONO;
    } else if (pWfx->Format.nChannels == 2) {
        pWfx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    } else {
        pWfx->dwChannelMask = 0; 
    }
    pWfx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    DbgPrint("DataRangeIntersection (Render): Success. Format: %uHz, %uch, %ubit\n", 
        pWfx->Format.nSamplesPerSec, pWfx->Format.nChannels, pWfx->Format.wBitsPerSample);
    
    return STATUS_SUCCESS;
}
NTSTATUS CMiniportTopologyLamaLoopbackRender::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object) { PAGED_CODE(); ASSERT(Object); DPF_ENTER(("[CMiniportTopologyLamaLoopbackRender::NonDelegatingQueryInterface]")); if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport)) { *Object = PVOID(PUNKNOWN(this)); } else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology)) { *Object = PVOID(PMINIPORTTOPOLOGY(this)); } else { *Object = NULL; return STATUS_NOT_SUPPORTED; } ((PUNKNOWN)*Object)->AddRef(); return STATUS_SUCCESS; }
NTSTATUS CreateMiniportTopologyLamaLoopbackRender(_Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType) { PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportTopologyLamaLoopbackRender]")); UNREFERENCED_PARAMETER(UnknownOuter); CMiniportTopologyLamaLoopbackRender *obj = new (PoolType, LAMA_POOL_TAG) CMiniportTopologyLamaLoopbackRender; if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; } *Unknown = PUNKNOWN((PMINIPORTTOPOLOGY)obj); (*Unknown)->AddRef(); return STATUS_SUCCESS; }

//=============================================================================
// CMiniportWaveRTLamaLoopbackRender - Modified
//=============================================================================
CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender(void) 
    : CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL), m_MiniportInstanceIndex(MAXULONG) // Initialize m_MiniportInstanceIndex
{ PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::CMiniportWaveRTLamaLoopbackRender]")); }

CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender(void) 
{ PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::~CMiniportWaveRTLamaLoopbackRender]")); if (m_Port) { m_Port->Release(); m_Port = NULL; } if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; } }

NTSTATUS CMiniportWaveRTLamaLoopbackRender::Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port_) 
{ 
    PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_); 
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::Init]")); 
    UNREFERENCED_PARAMETER(ResourceList); 
    m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef(); 
    m_Port = Port_; m_Port->AddRef(); 

    // Retrieve and store instance index
    IPortClsSubdeviceEx* pSubdeviceEx = NULL;
    NTSTATUS nameQueryStatus = m_UnknownAdapter->QueryInterface(IID_IPortClsSubdeviceEx, (PVOID*)&pSubdeviceEx);
    if (NT_SUCCESS(nameQueryStatus) && pSubdeviceEx)
    {
        PDEVICE_OBJECT pFilterDeviceObject = pSubdeviceEx->GetDeviceObject();
        POBJECT_NAME_INFORMATION pObjectNameInfo = NULL;
        ULONG returnLength = 0;

        nameQueryStatus = ObQueryNameString(pFilterDeviceObject, NULL, 0, &returnLength);
        if ((nameQueryStatus == STATUS_INFO_LENGTH_MISMATCH || nameQueryStatus == STATUS_BUFFER_TOO_SMALL) && returnLength > 0) // STATUS_BUFFER_TOO_SMALL can also be returned
        {
            pObjectNameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(PagedPool, returnLength, LAMA_POOL_TAG);
            if (pObjectNameInfo)
            {
                nameQueryStatus = ObQueryNameString(pFilterDeviceObject, pObjectNameInfo, returnLength, &returnLength);
                if (NT_SUCCESS(nameQueryStatus) && pObjectNameInfo->Name.Buffer != NULL && pObjectNameInfo->Name.Length > 0)
                {
                    DPF(DPF_LEVEL_INFO, ("Render Miniport FDO Name: %wZ", &pObjectNameInfo->Name));
                    USHORT len = pObjectNameInfo->Name.Length / sizeof(WCHAR);
                    // Simple parsing: assumes name ends with a single digit number.
                    if (len > 0 && pObjectNameInfo->Name.Buffer[len-1] >= L'0' && pObjectNameInfo->Name.Buffer[len-1] <= L'9')
                    {
                        this->m_MiniportInstanceIndex = pObjectNameInfo->Name.Buffer[len-1] - L'0';
                        DPF(DPF_LEVEL_INFO, ("Parsed Render Instance Index: %u", this->m_MiniportInstanceIndex));
                    } else {
                         DPF(DPF_LEVEL_WARNING, ("Could not parse instance index from render FDO name: %wZ. Defaulting to 0.", &pObjectNameInfo->Name));
                         this->m_MiniportInstanceIndex = 0; 
                    }
                } else { DPF(DPF_LEVEL_ERROR, ("ObQueryNameString (2nd call) failed: 0x%x. Defaulting to 0.", nameQueryStatus)); this->m_MiniportInstanceIndex = 0; }
                ExFreePoolWithTag(pObjectNameInfo, LAMA_POOL_TAG);
            } else { DPF(DPF_LEVEL_ERROR, ("Failed to allocate memory for ObjectNameInfo (Render). Defaulting to 0.")); this->m_MiniportInstanceIndex = 0; }
        } else { DPF(DPF_LEVEL_ERROR, ("ObQueryNameString (1st call) failed or zero length: 0x%x. Defaulting to 0.", nameQueryStatus)); this->m_MiniportInstanceIndex = 0;}
        pSubdeviceEx->Release();
    } else { DPF(DPF_LEVEL_ERROR, ("Failed to get IPortClsSubdeviceEx (Render): 0x%x. Defaulting to 0.", nameQueryStatus)); this->m_MiniportInstanceIndex = 0;}
    
    if (this->m_MiniportInstanceIndex >= MAX_LAMA_INSTANCES) {
        DPF(DPF_LEVEL_ERROR, ("Parsed instance index %u is out of bounds. Defaulting to 0.", this->m_MiniportInstanceIndex));
        this->m_MiniportInstanceIndex = 0;
    }
    return STATUS_SUCCESS; 
}

NTSTATUS CMiniportWaveRTLamaLoopbackRender::NewStream(_Out_ PMINIPORTWAVERTSTREAM * Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat) 
{ 
    PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::NewStream] Pin: %d, Capture: %d", Pin, Capture)); 
    ASSERT (Stream); ASSERT (DataFormat); ASSERT (PortStream); NTSTATUS ntStatus = STATUS_SUCCESS; 
    PCMiniportWaveRTLamaLoopbackStream newStream = NULL; *Stream = NULL; 
    newStream = new (NonPagedPoolNx, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackStream(NULL); 
    if (newStream == NULL) { DPF(DPF_LEVEL_ERROR, ("Render NewStream: Failed to allocate CMiniportWaveRTLamaLoopbackStream")); ntStatus = STATUS_INSUFFICIENT_RESOURCES; goto Done; } 
    
    ULONG instanceIdx = this->m_MiniportInstanceIndex; // Use the stored index
    DPF(DPF_LEVEL_INFO, ("Render NewStream: Using determined InstanceIndex %u", instanceIdx));
    
    ntStatus = newStream->Init(PortStream, DataFormat, FALSE, instanceIdx); /*Capture is FALSE for render*/ 
    if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("Render NewStream: Failed to initialize CMiniportWaveRTLamaLoopbackStream: 0x%x", ntStatus)); goto Done; } 
    ntStatus = newStream->QueryInterface(IID_IMiniportWaveRTStream, (PVOID*)Stream); 
    if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("Render NewStream: QueryInterface for IMiniportWaveRTStream failed: 0x%x", ntStatus)); goto Done; } 
Done: 
    if (!NT_SUCCESS(ntStatus)) { if (newStream) { newStream->Release(); } if (Stream != NULL) { *Stream = NULL; } } 
    else { if (newStream) { newStream->Release(); } } 
    DPF_LEAVE(("[CMiniportWaveRTLamaLoopbackRender::NewStream] ntStatus=0x%08x", ntStatus)); return ntStatus; 
}
NTSTATUS CMiniportWaveRTLamaLoopbackRender::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object) { PAGED_CODE(); ASSERT(Object); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackRender::NonDelegatingQueryInterface]")); if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport)) { *Object = PVOID(PUNKNOWN(this)); } else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT)) { *Object = PVOID(PMINIPORTWAVERT(this)); } else { *Object = NULL; return STATUS_NOT_SUPPORTED; } ((PUNKNOWN)*Object)->AddRef(); return STATUS_SUCCESS; }
NTSTATUS CreateMiniportWaveRTLamaLoopbackRender(_Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType) { PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportWaveRTLamaLoopbackRender]")); UNREFERENCED_PARAMETER(UnknownOuter); UNREFERENCED_PARAMETER(PoolType); CMiniportWaveRTLamaLoopbackRender *obj = new (NonPagedPoolNx, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackRender; if (NULL == obj) { DPF(DPF_LEVEL_ERROR, ("Failed to allocate CMiniportWaveRTLamaLoopbackRender")); return STATUS_INSUFFICIENT_RESOURCES; } *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj); (*Unknown)->AddRef(); return STATUS_SUCCESS; }

//=============================================================================
// LamaRenderPinWrite - Existing IRP_MJ_WRITE handler for Render Pin
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS LamaRenderPinWrite(_In_ PKSPIN Pin, _In_ PIRP Irp) { /* ... existing full implementation ... */ 
    PAGED_CODE(); ASSERT(Pin); ASSERT(Irp); DPF_ENTER(("[LamaRenderPinWrite] Pin=0x%p, Irp=0x%p", Pin, Irp));
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST; PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp); PVOID dataBuffer = NULL; ULONG dataLength = 0;
    PCMiniportWaveRTLamaLoopbackStream pStream = reinterpret_cast<PCMiniportWaveRTLamaLoopbackStream>(Pin->Context);
    if (!pStream) { DPF(DPF_LEVEL_ERROR, ("LamaRenderPinWrite: pStream context is NULL! Pin->Context=0x%p", Pin->Context)); status = STATUS_INVALID_DEVICE_STATE; Irp->IoStatus.Information = 0; goto Exit; }
    dataLength = irpStack->Parameters.Write.Length;
    if (dataLength == 0) { DPF(DPF_LEVEL_INFO, ("LamaRenderPinWrite: Zero length write.")); status = STATUS_SUCCESS; Irp->IoStatus.Information = 0; goto Exit; }
    if (Irp->MdlAddress != NULL) { dataBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPoolPriority); if (!dataBuffer) { DPF(DPF_LEVEL_ERROR, ("LamaRenderPinWrite: MmGetSystemAddressForMdlSafe failed for MdlAddress=0x%p", Irp->MdlAddress)); status = STATUS_INSUFFICIENT_RESOURCES; Irp->IoStatus.Information = 0; goto Exit; } }
    else if (Irp->AssociatedIrp.SystemBuffer != NULL) { dataBuffer = Irp->AssociatedIrp.SystemBuffer; }
    else { DPF(DPF_LEVEL_ERROR, ("LamaRenderPinWrite: No data buffer found in IRP.")); status = STATUS_INVALID_PARAMETER; Irp->IoStatus.Information = 0; goto Exit; }
    DPF(DPF_LEVEL_VERBOSE, ("LamaRenderPinWrite: DataLen=%u, DataBuf=0x%p", dataLength, dataBuffer));
    status = pStream->HandleWriteIRPData(dataBuffer, dataLength);
    if (NT_SUCCESS(status)) { Irp->IoStatus.Information = dataLength; } else { Irp->IoStatus.Information = 0; }
Exit: Irp->IoStatus.Status = status; IoCompleteRequest(Irp, IO_NO_INCREMENT); DPF_LEAVE(("[LamaRenderPinWrite] Status=0x%08x, Info=%Iu", status, Irp->IoStatus.Information)); return status;
}
#pragma code_seg()

//=============================================================================
// CMiniportTopologyLamaLoopbackCapture - Existing
//=============================================================================
#pragma code_seg("PAGE")
CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture(void) : CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::CMiniportTopologyLamaLoopbackCapture]")); }
CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture(void)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::~CMiniportTopologyLamaLoopbackCapture]")); if (m_Port) { m_Port->Release(); m_Port = NULL; } if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; } }
STDMETHODIMP_(NTSTATUS) CMiniportTopologyLamaLoopbackCapture::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{ PAGED_CODE(); ASSERT(Object); if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport)) { *Object = PVOID(PUNKNOWN(this)); } else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology)) { *Object = PVOID(PMINIPORTTOPOLOGY(this)); } else { *Object = NULL; return STATUS_NOT_SUPPORTED; } ((PUNKNOWN)*Object)->AddRef(); return STATUS_SUCCESS; }
NTSTATUS CMiniportTopologyLamaLoopbackCapture::Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY Port_)
{ PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_); DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::Init]")); UNREFERENCED_PARAMETER(ResourceList); m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef(); m_Port = Port_; m_Port->AddRef(); return STATUS_SUCCESS; }
NTSTATUS CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection(_In_ ULONG PinId, _In_ PKSDATARANGE DataRange, _In_ PKSDATARANGE MatchingDataRange, _In_ ULONG OutputBufferLength, _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat, _Out_ PULONG ResultantFormatLength)
{   /* ... existing full implementation ... */ 
    PAGED_CODE(); DPF_ENTER(("[CMiniportTopologyLamaLoopbackCapture::DataRangeIntersection] PinId=%u", PinId)); UNREFERENCED_PARAMETER(MatchingDataRange); if (!DataRange || !ResultantFormatLength) { return STATUS_INVALID_PARAMETER; } *ResultantFormatLength = 0; 
    if (!IsEqualGUIDAligned(DataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) || !IsEqualGUIDAligned(DataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) || !IsEqualGUIDAligned(DataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): No match due to Major/Sub/Specifier GUIDs.")); return STATUS_NO_MATCH; }
    PKSDATARANGE_AUDIO clientDataRangeAudio = (PKSDATARANGE_AUDIO)DataRange; ULONG clientMinChannels = clientDataRangeAudio->MinimumChannels; ULONG clientMaxChannels = clientDataRangeAudio->MaximumChannels; ULONG resultChannels;
    if (clientMaxChannels == 0 || clientMaxChannels > MAX_CHANNELS_PCM) { clientMaxChannels = MAX_CHANNELS_PCM; } if (clientMinChannels > clientMaxChannels) { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Client min channels %u > client max channels %u. No match.", clientMinChannels, clientMaxChannels)); return STATUS_NO_MATCH; }
    if (clientMaxChannels >= MIN_CHANNELS_PCM && clientMaxChannels <= MAX_CHANNELS_PCM) { resultChannels = clientMaxChannels; } else if (clientMinChannels >= MIN_CHANNELS_PCM && clientMinChannels <= MAX_CHANNELS_PCM) { resultChannels = clientMinChannels; } else { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Client channel range [%u, %u] is outside device capabilities [%u, %u]. No match.", clientMinChannels, clientMaxChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM)); return STATUS_NO_MATCH; }
    if (resultChannels < MIN_CHANNELS_PCM || resultChannels > MAX_CHANNELS_PCM) { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Selected resultChannels %u is outside device capabilities [%u, %u]. No match.", resultChannels, MIN_CHANNELS_PCM, MAX_CHANNELS_PCM)); return STATUS_NO_MATCH; }
    ULONG resultBitsPerSample = g_CurrentGlobalBitsPerSample; ULONG resultSampleRate = g_CurrentGlobalSampleRate;      
    if (resultBitsPerSample < clientDataRangeAudio->MinimumBitsPerSample || resultBitsPerSample > clientDataRangeAudio->MaximumBitsPerSample) { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Global BPS %u not in client range [%u, %u]", resultBitsPerSample, clientDataRangeAudio->MinimumBitsPerSample, clientDataRangeAudio->MaximumBitsPerSample)); return STATUS_NO_MATCH; }
    if (resultSampleRate < clientDataRangeAudio->MinimumSampleRate || resultSampleRate > clientDataRangeAudio->MaximumSampleRate) { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Global SR %u not in client range [%u, %u]", resultSampleRate, clientDataRangeAudio->MinimumSampleRate, clientDataRangeAudio->MaximumSampleRate)); return STATUS_NO_MATCH; }
    if (resultBitsPerSample < MIN_BITS_PER_SAMPLE_PCM || resultBitsPerSample > MAX_BITS_PER_SAMPLE_PCM || resultSampleRate < MIN_SAMPLE_RATE_PCM || resultSampleRate > MAX_SAMPLE_RATE_PCM) { DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Resulting format SR/BPS (%uHz, %ubit) outside device capabilities.", resultSampleRate, resultBitsPerSample)); return STATUS_NO_MATCH; }
    if (!ResultantFormat) { *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE); return STATUS_BUFFER_OVERFLOW; } if (OutputBufferLength < sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)) { DPF(DPF_LEVEL_ERROR, ("DataRangeIntersection (Capture): Output buffer too small. Needed %u, Got %u", (ULONG)sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), OutputBufferLength)); return STATUS_BUFFER_TOO_SMALL; }
    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pResFormatWfx = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ResultantFormat; pResFormatWfx->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE); pResFormatWfx->DataFormat.Flags = 0; pResFormatWfx->DataFormat.SampleSize = (resultBitsPerSample / 8) * resultChannels; pResFormatWfx->DataFormat.Reserved = 0; pResFormatWfx->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO; pResFormatWfx->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM; pResFormatWfx->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
    WAVEFORMATEXTENSIBLE *pWfx = &pResFormatWfx->WaveFormatExt; pWfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE; pWfx->Format.nChannels = (WORD)resultChannels; pWfx->Format.nSamplesPerSec = resultSampleRate; pWfx->Format.wBitsPerSample = (WORD)resultBitsPerSample; pWfx->Format.nBlockAlign = (WORD)((pWfx->Format.nChannels * pWfx->Format.wBitsPerSample) / 8); pWfx->Format.nAvgBytesPerSec = pWfx->Format.nSamplesPerSec * pWfx->Format.nBlockAlign; pWfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX); pWfx->Samples.wValidBitsPerSample = pWfx->Format.wBitsPerSample; 
    if (pWfx->Format.nChannels == 1) { pWfx->dwChannelMask = KSAUDIO_SPEAKER_MONO; } else if (pWfx->Format.nChannels == 2) { pWfx->dwChannelMask = KSAUDIO_SPEAKER_STEREO; } else { pWfx->dwChannelMask = 0; } pWfx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    *ResultantFormatLength = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE); DPF(DPF_LEVEL_INFO, ("DataRangeIntersection (Capture): Success. Format: %uHz, %uch, %ubit", pWfx->Format.nSamplesPerSec, pWfx->Format.nChannels, pWfx->Format.wBitsPerSample)); return STATUS_SUCCESS;
}
#pragma code_seg()

//=============================================================================
// CreateMiniportTopologyLamaLoopbackCapture - Existing Factory
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CreateMiniportTopologyLamaLoopbackCapture(_Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{ PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportTopologyLamaLoopbackCapture]")); UNREFERENCED_PARAMETER(UnknownOuter); PCMiniportTopologyLamaLoopbackCapture obj = new (PoolType, LAMA_POOL_TAG) CMiniportTopologyLamaLoopbackCapture; if (NULL == obj) { return STATUS_INSUFFICIENT_RESOURCES; } *Unknown = PUNKNOWN((PMINIPORTTOPOLOGY)obj); (*Unknown)->AddRef(); return STATUS_SUCCESS; }
#pragma code_seg()

//=============================================================================
// CMiniportWaveRTLamaLoopbackCapture - Modified
//=============================================================================
#pragma code_seg("PAGE")
CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture(void) 
    : CUnknown(NULL), m_Port(NULL), m_UnknownAdapter(NULL), m_MiniportInstanceIndex(MAXULONG) // Initialize m_MiniportInstanceIndex
{ PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::CMiniportWaveRTLamaLoopbackCapture]")); }

CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture(void)
{ PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::~CMiniportWaveRTLamaLoopbackCapture]")); if (m_Port) { m_Port->Release(); m_Port = NULL; } if (m_UnknownAdapter) { m_UnknownAdapter->Release(); m_UnknownAdapter = NULL; } }

STDMETHODIMP_(NTSTATUS) CMiniportWaveRTLamaLoopbackCapture::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID * Object)
{ PAGED_CODE(); ASSERT(Object); if (IsEqualGUIDAligned(Interface, IID_IUnknown) || IsEqualGUIDAligned(Interface, IID_IMiniport)) { *Object = PVOID(PUNKNOWN(this)); } else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT)) { *Object = PVOID(PMINIPORTWAVERT(this)); } else { *Object = NULL; return STATUS_NOT_SUPPORTED; } ((PUNKNOWN)*Object)->AddRef(); return STATUS_SUCCESS; }

NTSTATUS CMiniportWaveRTLamaLoopbackCapture::Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port_)
{ 
    PAGED_CODE(); ASSERT(UnknownAdapter); ASSERT(Port_); 
    DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::Init]")); 
    UNREFERENCED_PARAMETER(ResourceList); 
    m_UnknownAdapter = UnknownAdapter; m_UnknownAdapter->AddRef(); 
    m_Port = Port_; m_Port->AddRef(); 

    // Retrieve and store instance index
    IPortClsSubdeviceEx* pSubdeviceEx = NULL;
    NTSTATUS nameQueryStatus = m_UnknownAdapter->QueryInterface(IID_IPortClsSubdeviceEx, (PVOID*)&pSubdeviceEx);
    if (NT_SUCCESS(nameQueryStatus) && pSubdeviceEx)
    {
        PDEVICE_OBJECT pFilterDeviceObject = pSubdeviceEx->GetDeviceObject();
        POBJECT_NAME_INFORMATION pObjectNameInfo = NULL;
        ULONG returnLength = 0;

        nameQueryStatus = ObQueryNameString(pFilterDeviceObject, NULL, 0, &returnLength);
        if ((nameQueryStatus == STATUS_INFO_LENGTH_MISMATCH || nameQueryStatus == STATUS_BUFFER_TOO_SMALL) && returnLength > 0)
        {
            pObjectNameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(PagedPool, returnLength, LAMA_POOL_TAG);
            if (pObjectNameInfo)
            {
                nameQueryStatus = ObQueryNameString(pFilterDeviceObject, pObjectNameInfo, returnLength, &returnLength);
                if (NT_SUCCESS(nameQueryStatus) && pObjectNameInfo->Name.Buffer != NULL && pObjectNameInfo->Name.Length > 0)
                {
                    DPF(DPF_LEVEL_INFO, ("Capture Miniport FDO Name: %wZ", &pObjectNameInfo->Name));
                    USHORT len = pObjectNameInfo->Name.Length / sizeof(WCHAR);
                     // Simple parsing: assumes name ends with a single digit number.
                    if (len > 0 && pObjectNameInfo->Name.Buffer[len-1] >= L'0' && pObjectNameInfo->Name.Buffer[len-1] <= L'9')
                    {
                        this->m_MiniportInstanceIndex = pObjectNameInfo->Name.Buffer[len-1] - L'0';
                        DPF(DPF_LEVEL_INFO, ("Parsed Capture Instance Index: %u", this->m_MiniportInstanceIndex));
                    } else {
                         DPF(DPF_LEVEL_WARNING, ("Could not parse instance index from capture FDO name: %wZ. Defaulting to 0.", &pObjectNameInfo->Name));
                         this->m_MiniportInstanceIndex = 0; 
                    }
                } else { DPF(DPF_LEVEL_ERROR, ("ObQueryNameString (2nd call) failed: 0x%x. Defaulting to 0.", nameQueryStatus)); this->m_MiniportInstanceIndex = 0; }
                ExFreePoolWithTag(pObjectNameInfo, LAMA_POOL_TAG);
            } else { DPF(DPF_LEVEL_ERROR, ("Failed to allocate memory for ObjectNameInfo (Capture). Defaulting to 0.")); this->m_MiniportInstanceIndex = 0; }
        } else { DPF(DPF_LEVEL_ERROR, ("ObQueryNameString (1st call) failed or zero length: 0x%x. Defaulting to 0.", nameQueryStatus)); this->m_MiniportInstanceIndex = 0;}
        pSubdeviceEx->Release();
    } else { DPF(DPF_LEVEL_ERROR, ("Failed to get IPortClsSubdeviceEx (Capture): 0x%x. Defaulting to 0.", nameQueryStatus)); this->m_MiniportInstanceIndex = 0;}

    if (this->m_MiniportInstanceIndex >= MAX_LAMA_INSTANCES) {
        DPF(DPF_LEVEL_ERROR, ("Parsed instance index %u is out of bounds. Defaulting to 0.", this->m_MiniportInstanceIndex));
        this->m_MiniportInstanceIndex = 0;
    }
    return STATUS_SUCCESS; 
}

NTSTATUS CMiniportWaveRTLamaLoopbackCapture::NewStream(_Out_ PMINIPORTWAVERTSTREAM * Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat)
{   
    PAGED_CODE(); DPF_ENTER(("[CMiniportWaveRTLamaLoopbackCapture::NewStream] Pin: %d, ExpectedCaptureFromPortCls: %d", Pin, Capture));
    ASSERT (Stream); ASSERT (DataFormat); ASSERT (PortStream); NTSTATUS ntStatus = STATUS_SUCCESS;
    PCMiniportWaveRTLamaLoopbackStream newStream = new (NonPagedPoolNx, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackStream(NULL);
    if (newStream == NULL) { DPF(DPF_LEVEL_ERROR, ("Capture NewStream: Failed to allocate CMiniportWaveRTLamaLoopbackStream")); ntStatus = STATUS_INSUFFICIENT_RESOURCES; goto Done; }
    
    ULONG instanceIdx = this->m_MiniportInstanceIndex; // Use the stored index
    DPF(DPF_LEVEL_INFO, ("Capture NewStream: Using determined InstanceIndex %u", instanceIdx));
    
    ntStatus = newStream->Init(PortStream, DataFormat, TRUE, instanceIdx); // TRUE for Capture stream, pass instance index
    if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("Capture NewStream: Failed to initialize CMiniportWaveRTLamaLoopbackStream: 0x%x", ntStatus)); goto Done; }
    ntStatus = newStream->QueryInterface(IID_IMiniportWaveRTStream, (PVOID*)Stream);
    if (!NT_SUCCESS(ntStatus)) { DPF(DPF_LEVEL_ERROR, ("Capture NewStream: QueryInterface for IMiniportWaveRTStream failed: 0x%x", ntStatus)); goto Done; }
Done:
    if (!NT_SUCCESS(ntStatus)) { if (newStream) { newStream->Release(); } if (Stream != NULL) { *Stream = NULL; } }
    else { if (newStream) { newStream->Release(); } } 
    DPF_LEAVE(("[CMiniportWaveRTLamaLoopbackCapture::NewStream] ntStatus=0x%08x", ntStatus)); return ntStatus;
}
#pragma code_seg()

//=============================================================================
// CreateMiniportWaveRTLamaLoopbackCapture - Existing Factory
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CreateMiniportWaveRTLamaLoopbackCapture(_Out_ PUNKNOWN * Unknown, _In_ REFCLSID, _In_opt_ PUNKNOWN UnknownOuter, _In_ POOL_TYPE PoolType)
{ PAGED_CODE(); ASSERT(Unknown); DPF_ENTER(("[CreateMiniportWaveRTLamaLoopbackCapture]")); UNREFERENCED_PARAMETER(UnknownOuter); UNREFERENCED_PARAMETER(PoolType); PCMiniportWaveRTLamaLoopbackCapture obj = new (NonPagedPoolNx, LAMA_POOL_TAG) CMiniportWaveRTLamaLoopbackCapture; if (NULL == obj) { DPF(DPF_LEVEL_ERROR, ("Failed to allocate CMiniportWaveRTLamaLoopbackCapture")); return STATUS_INSUFFICIENT_RESOURCES; } *Unknown = PUNKNOWN((PMINIPORTWAVERT)obj); (*Unknown)->AddRef(); return STATUS_SUCCESS; }
#pragma code_seg()

//=============================================================================
// LamaCapturePinRead - Existing IRP_MJ_READ handler for Capture Pin
//=============================================================================
#pragma code_seg("PAGE") 
NTSTATUS LamaCapturePinRead(_In_ PKSPIN Pin, _In_ PIRP Irp)
{
    PAGED_CODE(); 
    ASSERT(Pin); ASSERT(Irp);
    DPF_ENTER(("[LamaCapturePinRead] Pin=0x%p, Irp=0x%p", Pin, Irp));

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PVOID dataBuffer = NULL;
    ULONG dataLength = 0;
    ULONG bytesCopied = 0;

    PCMiniportWaveRTLamaLoopbackStream pStream = reinterpret_cast<PCMiniportWaveRTLamaLoopbackStream>(Pin->Context);

    if (!pStream)
    {
        DPF(DPF_LEVEL_ERROR, ("LamaCapturePinRead: pStream context is NULL! Pin->Context=0x%p", Pin->Context));
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }
                
    dataLength = irpStack->Parameters.Read.Length;

    if (dataLength == 0)
    {
        DPF(DPF_LEVEL_INFO, ("LamaCapturePinRead: Zero length read request."));
        status = STATUS_SUCCESS; 
        bytesCopied = 0;
        goto Exit;
    }

    if (Irp->MdlAddress != NULL)
    {
        dataBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPoolPriority);
        if (!dataBuffer)
        {
            DPF(DPF_LEVEL_ERROR, ("LamaCapturePinRead: MmGetSystemAddressForMdlSafe failed for MdlAddress=0x%p", Irp->MdlAddress));
            status = STATUS_INSUFFICIENT_RESOURCES;
            bytesCopied = 0;
            goto Exit;
        }
    }
    else if (Irp->AssociatedIrp.SystemBuffer != NULL) 
    {
        dataBuffer = Irp->AssociatedIrp.SystemBuffer;
    }
    else
    {
        DPF(DPF_LEVEL_ERROR, ("LamaCapturePinRead: No data buffer found in IRP (MdlAddress and SystemBuffer are NULL)."));
        status = STATUS_INVALID_PARAMETER;
        bytesCopied = 0;
        goto Exit;
    }
    
    DPF(DPF_LEVEL_VERBOSE, ("LamaCapturePinRead: DataLenReq=%u, DataBuf=0x%p", dataLength, dataBuffer));
    status = pStream->HandleReadIRPData(dataBuffer, dataLength, &bytesCopied);
    
Exit:
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesCopied; 
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    DPF_LEAVE(("[LamaCapturePinRead] Status=0x%08x, Info=%Iu", status, Irp->IoStatus.Information));
    return status;
}
#pragma code_seg()








