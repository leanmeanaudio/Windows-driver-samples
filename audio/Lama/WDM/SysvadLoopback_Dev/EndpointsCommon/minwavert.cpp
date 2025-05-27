/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minwavert.cpp

Abstract:

    Implementation of wavert miniport.

--*/

#pragma warning (disable : 4127)

#include <sysvad.h>
#include <limits.h>
#include <MsApoFxProxy.h>
#include "ContosoKeywordDetector.h"
#include "SysVadShared.h"
#include "simple.h"
#include "minwavert.h"
#include "minwavertstream.h"
#include "IHVPrivatePropertySet.h"
#include "AudioModuleHelper.h"
#include "Lama/SysvadLoopback/lamaloopbackcommon.h" // For Lama Loopback definitions


#define EFFECTS_LIST_COUNT 2

//=============================================================================
// CMiniportWaveRT
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CreateMiniportWaveRTSYSVAD
( 
    _Out_           PUNKNOWN                              * Unknown,
    _In_            REFCLSID,
    _In_opt_        PUNKNOWN                                UnknownOuter,
    _In_            POOL_FLAGS                              PoolFlags,
    _In_            PUNKNOWN                                UnknownAdapter,
    _In_opt_        PVOID                                   DeviceContext,
    _In_            PENDPOINT_MINIPAIR                      MiniportPair
)
/*++

Routine Description:

  Create the wavert miniport.

Arguments:

  Unknown - 

  RefClsId -

  UnknownOuter -

  PoolType -

  UnkownAdapter -

  DeviceContext -

  MiniportPair -

Return Value:

  NT status code.

--*/
{
    UNREFERENCED_PARAMETER(UnknownOuter);

    PAGED_CODE();

    ASSERT(Unknown);
    ASSERT(MiniportPair);

    CMiniportWaveRT *obj = new (PoolFlags, MINWAVERT_POOLTAG) CMiniportWaveRT
                                                             (
                                                                UnknownAdapter,
                                                                MiniportPair,
                                                                DeviceContext
                                                             );
    if (NULL == obj)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    obj->AddRef();
    *Unknown = reinterpret_cast<IUnknown*>(obj);

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
CMiniportWaveRT::~CMiniportWaveRT
( 
    void 
)
/*++

Routine Description:

  Destructor for wavert miniport

Arguments:

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::~CMiniportWaveRT]"));

    if (m_pDeviceFormat)
    {
        ExFreePoolWithTag( m_pDeviceFormat, MINWAVERT_POOLTAG );
        m_pDeviceFormat = NULL;
    }

    if (m_pMixFormat)
    {
        ExFreePoolWithTag( m_pMixFormat, MINWAVERT_POOLTAG );
        m_pMixFormat = NULL;
    }

    if (m_pbMuted)
    {
        ExFreePoolWithTag( m_pbMuted, MINWAVERT_POOLTAG );
        m_pbMuted = NULL;
    }

    if (m_plVolumeLevel)
    {
        ExFreePoolWithTag( m_plVolumeLevel, MINWAVERT_POOLTAG );
        m_plVolumeLevel = NULL;
    }

    if (m_plPeakMeter)
    {
        ExFreePoolWithTag( m_plPeakMeter, MINWAVERT_POOLTAG );
        m_plPeakMeter = NULL;
    }

    if (m_pDrmPort)
    {
        m_pDrmPort->Release();
        m_pDrmPort = NULL;
    }

    if (m_pPortEvents)
    {
        m_pPortEvents->Release();
        m_pPortEvents = NULL;
    }

    if (m_pPortClsNotifications)
    {
        m_pPortClsNotifications->Release();
        m_pPortClsNotifications = NULL;
    }
    
    if (m_SystemStreams)
    {
        ExFreePoolWithTag( m_SystemStreams, MINWAVERT_POOLTAG );
        m_SystemStreams = NULL;
    }

    if (m_OffloadStreams)
    {
        ExFreePoolWithTag( m_OffloadStreams, MINWAVERT_POOLTAG );
        m_OffloadStreams = NULL;
    }
    
    if (m_LoopbackStreams)
    {
        ExFreePoolWithTag( m_LoopbackStreams, MINWAVERT_POOLTAG );
        m_LoopbackStreams = NULL;
    }

    if (m_pAudioModules)
    {
        FreeStreamAudioModules(m_pAudioModules, GetAudioModuleListCount());
        m_pAudioModules = NULL;
    }

#if defined(SYSVAD_BTH_BYPASS) || defined(SYSVAD_USB_SIDEBAND)
    if (IsSidebandDevice())
    {
        m_pSidebandDevice->SetFormatChangeHandler(m_DeviceType, NULL, NULL);
        SAFE_RELEASE(m_pSidebandDevice);
    }
#endif // defined(SYSVAD_BTH_BYPASS) || defined(SYSVAD_USB_SIDEBAND)

} // ~CMiniportWaveRT

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::DataRangeIntersection
( 
    _In_        ULONG                       PinId,
    _In_        PKSDATARANGE                ClientDataRange,
    _In_        PKSDATARANGE                MyDataRange,
    _In_        ULONG                       OutputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                PVOID                       ResultantFormat,
    _Out_       PULONG                      ResultantFormatLength 
)
/*++

Routine Description:

  The DataRangeIntersection function determines the highest quality 
  intersection of two data ranges.

  This sample just validates the # of channels and lets the class handler
  do the rest.

Arguments:

  PinId -           Pin for which data intersection is being determined. 

  ClientDataRange - Pointer to KSDATARANGE structure which contains the data 
                    range submitted by client in the data range intersection 
                    property request. 

  MyDataRange -         Pin's data range to be compared with client's data 
                        range. In this case we actually ignore our own data 
                        range, because we know that we only support one range.

  OutputBufferLength -  Size of the buffer pointed to by the resultant format 
                        parameter. 

  ResultantFormat -     Pointer to value where the resultant format should be 
                        returned. 

  ResultantFormatLength -   Actual length of the resultant format placed in 
                            ResultantFormat. This should be less than or equal 
                            to OutputBufferLength. 

  Return Value:

    NT status code.

  Remarks:

    This sample driver's custom data intersection handler handles all the
    audio endpoints defined in this driver. Some endpoints support mono formats
    while others do not. The handler is written such that it requires an exact
    match in MaximumChannels. This simplifies the handler but requires the pin
    data ranges to include a separate data range for mono formats if the pin
    supports mono formats.

--*/
{
    ULONG                   requiredSize;

    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ResultantFormat);

    PAGED_CODE();

    if (!IsEqualGUIDAligned(ClientDataRange->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    requiredSize = sizeof (KSDATAFORMAT_WAVEFORMATEX);

    //
    // Validate return buffer size, if the request is only for the
    // size of the resultant structure, return it now before
    // returning other types of errors.
    //
    if (!OutputBufferLength) 
    {
        *ResultantFormatLength = requiredSize;
        return STATUS_BUFFER_OVERFLOW;
    } 
    else if (OutputBufferLength < requiredSize) 
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // For Lama Loopback, the sample rate is dynamic, so we can't rely on static MyDataRange.
    // IsFormatSupported will handle the detailed check.
    // For other devices, the original channel check remains.
    if (!(m_DeviceType == eLamaLoopbackRenderDevice || m_DeviceType == eLamaLoopbackCaptureDevice))
    {
        // Verify channel count is supported. This routine assumes a separate data
        // range for each supported channel count.
        if (((PKSDATARANGE_AUDIO)MyDataRange)->MaximumChannels != ((PKSDATARANGE_AUDIO)ClientDataRange)->MaximumChannels)
        {
            return STATUS_NO_MATCH;
        }
    }
    
    //
    // Ok, let the class handler do the rest.
    //
    return STATUS_NOT_IMPLEMENTED;
} // DataRangeIntersection

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::GetDescription
( 
    _Out_ PPCFILTER_DESCRIPTOR * OutFilterDescriptor 
)
/*++

Routine Description:

  The GetDescription function gets a pointer to a filter description. 
  It provides a location to deposit a pointer in miniport's description 
  structure.

Arguments:

  OutFilterDescriptor - Pointer to the filter description. 

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    *OutFilterDescriptor = &m_FilterDesc;

    return STATUS_SUCCESS;
} // GetDescription

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::Init
( 
    _In_  PUNKNOWN                UnknownAdapter_,
    _In_  PRESOURCELIST           ResourceList_,
    _In_  PPORTWAVERT             Port_ 
)
/*++

Routine Description:

  The Init function initializes the miniport. Callers of this function 
  should run at IRQL PASSIVE_LEVEL

Arguments:

  UnknownAdapter - A pointer to the Iuknown interface of the adapter object. 

  ResourceList - Pointer to the resource list to be supplied to the miniport 
                 during initialization. The port driver is free to examine the 
                 contents of the ResourceList. The port driver will not be 
                 modify the ResourceList contents. 

  Port - Pointer to the topology port object that is linked with this miniport. 

Return Value:

  NT status code.

--*/
{
    UNREFERENCED_PARAMETER(UnknownAdapter_);
    UNREFERENCED_PARAMETER(ResourceList_);
    UNREFERENCED_PARAMETER(Port_);
    PAGED_CODE();

    ASSERT(UnknownAdapter_);
    ASSERT(Port_);

    DPF_ENTER(("[CMiniportWaveRT::Init]"));

    NTSTATUS ntStatus = STATUS_SUCCESS;
    size_t   size;

    //
    // Init class data members
    //
    m_ulLoopbackAllocated               = 0;
    m_ulSystemAllocated                 = 0;
    m_ulOffloadAllocated                = 0;
    m_ulKeywordDetectorAllocated        = 0;
    m_SystemStreams                     = NULL;
    m_OffloadStreams                    = NULL;
    m_LoopbackStreams                   = NULL;
    m_bGfxEnabled                       = FALSE;
    m_pbMuted                           = NULL;
    m_plVolumeLevel                     = NULL;
    m_plPeakMeter                       = NULL;
    m_pMixFormat                        = NULL;
    m_pDeviceFormat                     = NULL;
    m_ulMixDrmContentId                 = 0;
    m_LoopbackProtection                = CONSTRICTOR_OPTION_DISABLE;
    RtlZeroMemory(&m_MixDrmRights, sizeof(m_MixDrmRights));

    // Initialize Lama Loopback specific members
    m_currentSampleRate = 48000; // Default sample rate
    RtlCopyMemory(&m_dynamicFormat16ch, &Pcm48000_16ch_16bit, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));


    // 
    // For port notification support.
    //
    if (!NT_SUCCESS(Port_->QueryInterface(IID_IPortClsNotifications, (PVOID *)&m_pPortClsNotifications)))
    {
        m_pPortClsNotifications = NULL;
    }

    //
    // Init all the modules associated to this miniport.
    //
    ULONG cModules = GetAudioModuleListCount();
    if (cModules)
    {
        //
        // Module list size.
        //
        size = cModules * sizeof(AUDIOMODULE);
        m_pAudioModules = (AUDIOMODULE *)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MINWAVERT_POOLTAG);
        if (m_pAudioModules == NULL)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        for (ULONG i=0; i<cModules; ++i)
        {
            PAUDIOMODULE_DESCRIPTOR moduleDesc = &m_pMiniportPair->ModuleList[i];

            //
            // Init run-time module element.
            //
            m_pAudioModules[i].Descriptor = moduleDesc;
            m_pAudioModules[i].Context    = NULL;
            m_pAudioModules[i].InstanceId = moduleDesc->InstanceId;
            m_pAudioModules[i].Enabled    = TRUE;
            //
            // Module context size.
            //
            size = moduleDesc->ContextSize;
            if (size)
            {
                m_pAudioModules[i].Context = 
                    ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MINWAVERT_POOLTAG);
                
                if (m_pAudioModules[i].Context == NULL)
                {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
            }

            
            if (moduleDesc->InitClass)
            {
                KSAUDIOMODULE_NOTIFICATION  NotificationHeader;
                NotificationHeader.ProviderId.DeviceId = *GetAudioModuleNotificationDeviceId();
                NotificationHeader.ProviderId.ClassId = *moduleDesc->ClassId;
                NotificationHeader.ProviderId.InstanceId = moduleDesc->InstanceId;
                
                ntStatus = moduleDesc->InitClass(moduleDesc,
                                                 m_pAudioModules[i].Context,
                                                 size,
                                                 &NotificationHeader,
                                                 m_pPortClsNotifications);
                if (!NT_SUCCESS(ntStatus))
                {
                    ASSERT(FALSE);
                    return ntStatus;
                }
            }
        }
    }

    //
    // Init the audio-engine used by the render devices.
    //
    if (IsRenderDevice())
    {
        if (m_ulMaxSystemStreams == 0 )
        {
            return STATUS_INVALID_DEVICE_STATE;
        }
            
        // System streams.
        size = sizeof(PCMiniportWaveRTStream) * m_ulMaxSystemStreams;
        m_SystemStreams = (PCMiniportWaveRTStream *)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MINWAVERT_POOLTAG);
        if (m_SystemStreams == NULL)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        if (IsLoopbackSupported())
        {
            if (m_ulMaxLoopbackStreams == 0)
            {
                return STATUS_INVALID_DEVICE_STATE;
            }

            // Loopback streams.
            size = sizeof(PCMiniportWaveRTStream) * m_ulMaxLoopbackStreams;
            m_LoopbackStreams = (PCMiniportWaveRTStream *)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MINWAVERT_POOLTAG);
            if (m_LoopbackStreams == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
        }

        if (IsOffloadSupported())
        {
            PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pDeviceFormats;
            ULONG numDeviceFormats;

            if (m_ulMaxOffloadStreams == 0)
            {
                return STATUS_INVALID_DEVICE_STATE;
            }
            
            // Offload streams.
            size = sizeof(PCMiniportWaveRTStream) * m_ulMaxOffloadStreams;
            m_OffloadStreams = (PCMiniportWaveRTStream *)ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MINWAVERT_POOLTAG);
            if (m_OffloadStreams == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            // Formats. 
            m_pDeviceFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), MINWAVERT_POOLTAG);
            if (m_pDeviceFormat == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            numDeviceFormats = GetAudioEngineSupportedDeviceFormats(&pDeviceFormats);

            if (numDeviceFormats < 1)
            {
                return STATUS_UNSUCCESSFUL;
            }
            RtlCopyMemory((PVOID)(m_pDeviceFormat), (PVOID)(pDeviceFormats), sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));

            m_pMixFormat = (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), MINWAVERT_POOLTAG);
            if (m_pMixFormat == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            m_pMixFormat->DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
            m_pMixFormat->DataFormat.Flags = 0;
            m_pMixFormat->DataFormat.Reserved = 0;
            m_pMixFormat->DataFormat.SampleSize = 0;
            m_pMixFormat->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
            m_pMixFormat->DataFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            m_pMixFormat->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
            m_pMixFormat->WaveFormatExt.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            m_pMixFormat->WaveFormatExt.Format.nChannels = 2;
            m_pMixFormat->WaveFormatExt.Format.nSamplesPerSec = 48000;
            m_pMixFormat->WaveFormatExt.Format.nBlockAlign = 4;
            m_pMixFormat->WaveFormatExt.Format.nAvgBytesPerSec = 192000;
            m_pMixFormat->WaveFormatExt.Format.wBitsPerSample = 16;
            m_pMixFormat->WaveFormatExt.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            m_pMixFormat->WaveFormatExt.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            m_pMixFormat->WaveFormatExt.Samples.wValidBitsPerSample = 16;
            m_pMixFormat->WaveFormatExt.dwChannelMask = KSAUDIO_SPEAKER_STEREO;

            m_bGfxEnabled = FALSE;

            m_pbMuted = (PBOOL)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_DeviceMaxChannels * sizeof(BOOL), MINWAVERT_POOLTAG);
            if (m_pbMuted == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            m_plVolumeLevel = (PLONG)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_DeviceMaxChannels * sizeof(LONG), MINWAVERT_POOLTAG);
            if (m_plVolumeLevel == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            m_plPeakMeter = (PLONG)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_DeviceMaxChannels * sizeof(LONG), MINWAVERT_POOLTAG);
            if (m_plPeakMeter == NULL)
            {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
        }
        
        // 
        // For DRM support.
        //
        if (!NT_SUCCESS(Port_->QueryInterface(IID_IDrmPort2, (PVOID *)&m_pDrmPort)))
        {
            m_pDrmPort = NULL;
        }
    }
    
    // 
    // For KS event support.
    //
    if (!NT_SUCCESS(Port_->QueryInterface(IID_IPortEvents, (PVOID *)&m_pPortEvents)))
    {
        m_pPortEvents = NULL;
    }
    
    // Format change handler is only required for bluetooth HFP implementation
#ifdef SYSVAD_BTH_BYPASS
    if (IsSidebandDevice())
    {
        PSIDEBANDDEVICECOMMON sidebandDevice = NULL;
        
        sidebandDevice = GetSidebandDevice(); // weak ref.
        ASSERT(sidebandDevice != NULL);
        
        //
        // Register with BthHfpDevice to get notification events.
        //
        sidebandDevice->SetFormatChangeHandler(
            m_DeviceType,
            EvtFormatChangeHandler,             // handler
            PCMiniportWaveRT(this));            // context.
    }
#endif  // #ifdef SYSVAD_BTH_BYPASS

    return ntStatus;
} // Init

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::NewStream
( 
    _Out_ PMINIPORTWAVERTSTREAM * OutStream,
    _In_  PPORTWAVERTSTREAM       OuterUnknown,
    _In_  ULONG                   Pin,
    _In_  BOOLEAN                 Capture,
    _In_  PKSDATAFORMAT           DataFormat
)
/*++

Routine Description:

  The NewStream function creates a new instance of a logical stream 
  associated with a specified physical channel. Callers of NewStream should 
  run at IRQL PASSIVE_LEVEL.

Arguments:

  OutStream -

  OuterUnknown -

  Pin - 

  Capture - 

  DataFormat -

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(OutStream);
    ASSERT(DataFormat);

    DPF_ENTER(("[CMiniportWaveRT::NewStream]"));

    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PCMiniportWaveRTStream      stream = NULL;
    GUID                        signalProcessingMode = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;
    
    *OutStream = NULL;

     //
    // If the data format attributes were specified, extract them.
    //
    if ( DataFormat->Flags & KSDATAFORMAT_ATTRIBUTES )
    {
        // The attributes are aligned (QWORD alignment) after the data format
        PKSMULTIPLE_ITEM attributes = (PKSMULTIPLE_ITEM) (((PBYTE)DataFormat) + ((DataFormat->FormatSize + FILE_QUAD_ALIGNMENT) & ~FILE_QUAD_ALIGNMENT));
        ntStatus = GetAttributesFromAttributeList(attributes, attributes->Size, &signalProcessingMode);
    }

    // Check if we have enough streams.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ValidateStreamCreate(Pin, Capture);
    }

    // Determine if the format is valid.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = IsFormatSupported(Pin, Capture, DataFormat);
    }

    // Instantiate a stream. Stream must be in
    // NonPagedPool(Nx) because of file saving.
    //
    if (NT_SUCCESS(ntStatus))
    {
        stream = new (POOL_FLAG_NON_PAGED, MINWAVERT_POOLTAG) 
            CMiniportWaveRTStream(NULL);

        if (stream)
        {
            stream->AddRef();

            ntStatus = 
                stream->Init
                ( 
                    this,
                    OuterUnknown,
                    Pin,
                    Capture,
                    DataFormat,
                    signalProcessingMode
                );
        }
        else
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        *OutStream = PMINIPORTWAVERTSTREAM(stream);
        (*OutStream)->AddRef();

        // The stream has references now for the caller.  The caller expects these
        // references to be there.
    }

    // This is our private reference to the stream.  The caller has
    // its own, so we can release in any case.
    //
    if (stream)
    {
        stream->Release();
    }
    
    return ntStatus;
} // NewStream

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRT::NonDelegatingQueryInterface
( 
    _In_ REFIID  Interface,
    _COM_Outptr_ PVOID * Object 
)
/*++

Routine Description:

  QueryInterface

Arguments:

  Interface - GUID

  Object - interface pointer to be returned.

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVERT(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRT))
    {
        *Object = PVOID(PMINIPORTWAVERT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportAudioSignalProcessing))
    {
        *Object = PVOID(PMINIPORTAudioSignalProcessing(this));
    }
    // In this sample, IMiniportAudioEngineNode is supported only for offloading endpoints.
    // at thso moment, offload could only be enabled for  render endpoints not capture.
    // Incorrectly support IMiniportAudioEngineNode interface by the miniport without underlying 
    // HWAudioEngine node will cause miniport::Init to fail.
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportAudioEngineNode) && IsOffloadSupported())
    {
         *Object = (PVOID)(IMiniportAudioEngineNode*)this;
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        // We reference the interface for the caller.

        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS) CMiniportWaveRT::GetDeviceDescription(_Out_ PDEVICE_DESCRIPTION DmaDeviceDescription)
{
    PAGED_CODE ();

    ASSERT (DmaDeviceDescription);

    DPF_ENTER(("[CMiniportWaveRT::GetDeviceDescription]"));

    RtlZeroMemory (DmaDeviceDescription, sizeof (DEVICE_DESCRIPTION));

    //
    // Init device description. This sample is using the same info for all m_DeviceType(s).
    // 
    
    DmaDeviceDescription->Master = TRUE;
    DmaDeviceDescription->ScatterGather = TRUE;
    DmaDeviceDescription->Dma32BitAddresses = TRUE;
    DmaDeviceDescription->InterfaceType = PCIBus;
    DmaDeviceDescription->MaximumLength = 0xFFFFFFFF;

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::GetModes
(
    _In_                                        ULONG     Pin,
    _Out_writes_opt_(*NumSignalProcessingModes) GUID*     SignalProcessingModes,
    _Inout_                                     ULONG*    NumSignalProcessingModes
)
/*

1.	If Pin is not a valid pin number, 
        return STATUS_INVALID_PARAMETER.
        
2.	If Pin is a valid pin number and it supports n modes (n>0), 
        init out-parameters and return STATUS_SUCCESS.
    
3.  Else this pin doesn't support any mode,
        return STATUS_NOT_SUPPORTED. 
        example: bridge pins or another mode-not-aware pins.

*/
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::GetModes]"));

    NTSTATUS                ntStatus    = STATUS_INVALID_PARAMETER;
    ULONG                   numModes    = 0;
    MODE_AND_DEFAULT_FORMAT *modeInfo   = NULL;

    if (Pin >= m_pMiniportPair->WaveDescriptor->PinCount)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // This method is valid only on the following pins:
    //  render is offload capable:
    //      sink (#0) and offload (#1) pins.
    //  render is NOT offload capable:
    //      sink (#0) pin.
    //  capture device: 
    //      source (#1) pin.
    //
    numModes = GetPinSupportedDeviceModes(Pin, &modeInfo);
    if (numModes == 0)
    {
        return STATUS_NOT_SUPPORTED;
    }
   
    // If caller requests the modes, verify sufficient buffer size then return the modes
    if (SignalProcessingModes != NULL)
    {
        if (*NumSignalProcessingModes < numModes)
        {
            *NumSignalProcessingModes = numModes;
            ntStatus = STATUS_BUFFER_TOO_SMALL;
            goto Done;
        }

        for (ULONG i=0; i<numModes; ++i)
        {
            SignalProcessingModes[i] = modeInfo[i].Mode;
        }
    }

    ASSERT(numModes > 0);
    *NumSignalProcessingModes = numModes;
    ntStatus = STATUS_SUCCESS;

Done:   
    return ntStatus;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::ValidateStreamCreate
(
    _In_    ULONG   _Pin,
    _In_    BOOLEAN _Capture
)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::ValidateStreamCreate]"));

    NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;

    if (_Capture)
    {
        if (IsLoopbackPin(_Pin))
        {
            VERIFY_PIN_INSTANCE_RESOURCES_AVAILABLE(ntStatus, m_ulLoopbackAllocated, m_ulMaxLoopbackStreams);
        }
        else if (IsSystemCapturePin(_Pin) || IsCellularBiDiCapturePin(_Pin))
        {
            VERIFY_PIN_INSTANCE_RESOURCES_AVAILABLE(ntStatus, m_ulSystemAllocated, m_ulMaxSystemStreams);
        }
        else if (IsKeywordDetectorPin(_Pin))
        {
            VERIFY_PIN_INSTANCE_RESOURCES_AVAILABLE(ntStatus, m_ulKeywordDetectorAllocated, m_ulMaxKeywordDetectorStreams);
        }
    }
    else
    {
        if (IsSystemRenderPin(_Pin))
        {
            VERIFY_PIN_INSTANCE_RESOURCES_AVAILABLE(ntStatus, m_ulSystemAllocated, m_ulMaxSystemStreams);
        }
        else if (IsOffloadPin(_Pin))
        {
            VERIFY_PIN_INSTANCE_RESOURCES_AVAILABLE(ntStatus, m_ulOffloadAllocated, m_ulMaxOffloadStreams);
        }
    }

    return ntStatus;
}

//=============================================================================
#pragma code_seg()
_Use_decl_annotations_
VOID CMiniportWaveRT::AcquireFormatsAndModesLock()
{
    KeAcquireSpinLock(&m_DeviceFormatsAndModesLock, &m_DeviceFormatsAndModesIrql);
}

#pragma code_seg()
_Use_decl_annotations_
VOID CMiniportWaveRT::ReleaseFormatsAndModesLock()
{
    KeReleaseSpinLock(&m_DeviceFormatsAndModesLock, m_DeviceFormatsAndModesIrql);
}

//---------------------------------------------------------------------------
// GetPinSupportedDeviceFormats 
//
//  Return supported formats for a given pin.
//
//  Return value
//      The number of KSDATAFORMAT_WAVEFORMATEXTENSIBLE items.
//
//  Remarks
//      Supported formats index array follows same order as filter's pin
//      descriptor list.
//
#pragma code_seg()
_Use_decl_annotations_
ULONG CMiniportWaveRT::GetPinSupportedDeviceFormats(_In_ ULONG PinId, _Outptr_opt_result_buffer_(return) KSDATAFORMAT_WAVEFORMATEXTENSIBLE **ppFormats)
{
    // For Lama Loopback, return the dynamic format.
    if (m_DeviceType == eLamaLoopbackRenderDevice || m_DeviceType == eLamaLoopbackCaptureDevice)
    {
        if (ppFormats != NULL)
        {
            *ppFormats = &m_dynamicFormat16ch;
        }
        return 1; // Only one dynamic format
    }
    
    PPIN_DEVICE_FORMATS_AND_MODES pDeviceFormatsAndModes = NULL;

    AcquireFormatsAndModesLock();

    pDeviceFormatsAndModes = m_DeviceFormatsAndModes;
    ASSERT(m_DeviceFormatsAndModesCount > PinId);
    ASSERT(pDeviceFormatsAndModes[PinId].WaveFormats != NULL);
    ASSERT(pDeviceFormatsAndModes[PinId].WaveFormatsCount > 0);

    if (ppFormats != NULL)
    {
        *ppFormats = pDeviceFormatsAndModes[PinId].WaveFormats;
    }

    ReleaseFormatsAndModesLock();

    return pDeviceFormatsAndModes[PinId].WaveFormatsCount;
}

//---------------------------------------------------------------------------
// GetAudioEngineSupportedDeviceFormats 
//
//  Return supported device formats for the audio engine node.
//
//  Return value
//      The number of KSDATAFORMAT_WAVEFORMATEXTENSIBLE items.
//
//  Remarks
//      Supported formats index array follows same order as filter's pin
//      descriptor list. This routine assumes the engine formats are the
//      last item in the filter's array of PIN_DEVICE_FORMATS_AND_MODES.
//
#pragma code_seg()
_Use_decl_annotations_
ULONG CMiniportWaveRT::GetAudioEngineSupportedDeviceFormats(_Outptr_opt_result_buffer_(return) KSDATAFORMAT_WAVEFORMATEXTENSIBLE **ppFormats)
{
    ULONG i;
    PPIN_DEVICE_FORMATS_AND_MODES pDeviceFormatsAndModes = NULL;

    AcquireFormatsAndModesLock();

    pDeviceFormatsAndModes = m_DeviceFormatsAndModes;

    // By convention, the audio engine node's device formats are the last
    // entry in the PIN_DEVICE_FORMATS_AND_MODES list.

    // Since this endpoint apparently supports offload, there must be at least a system,
    // offload, and loopback pin, plus the entry for the device formats.
    ASSERT(m_DeviceFormatsAndModesCount > 3);

    i = m_DeviceFormatsAndModesCount - 1;                       // Index of last list entry

    ASSERT(pDeviceFormatsAndModes[i].PinType == NoPin);
    ASSERT(pDeviceFormatsAndModes[i].WaveFormats != NULL);
    ASSERT(pDeviceFormatsAndModes[i].WaveFormatsCount > 0);

    if (ppFormats != NULL)
    {
        *ppFormats = pDeviceFormatsAndModes[i].WaveFormats;
    }

    ReleaseFormatsAndModesLock();
    return pDeviceFormatsAndModes[i].WaveFormatsCount;
}

//---------------------------------------------------------------------------
// GetPinSupportedDeviceModes 
//
//  Return mode information for a given pin.
//
//  Return value
//      The number of MODE_AND_DEFAULT_FORMAT items or 0 if none.
//
//  Remarks
//      Supported formats index array follows same order as filter's pin
//      descriptor list.
//
#pragma code_seg()
_Use_decl_annotations_
ULONG CMiniportWaveRT::GetPinSupportedDeviceModes(_In_ ULONG PinId, _Outptr_opt_result_buffer_(return) _On_failure_(_Deref_post_null_) MODE_AND_DEFAULT_FORMAT **ppModes)
{
    PMODE_AND_DEFAULT_FORMAT modes;
    ULONG numModes;

    AcquireFormatsAndModesLock();

    ASSERT(m_DeviceFormatsAndModesCount > PinId);
    ASSERT((m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormatCount == 0) == (m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormat == NULL));

    modes = m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormat;
    numModes = m_DeviceFormatsAndModes[PinId].ModeAndDefaultFormatCount;

#ifdef SYSVAD_BTH_BYPASS
    // Special handling for the SCO bypass endpoint, whose modes are determined at runtime
    if (m_DeviceType == eBthHfpMicDevice)
    {
        ASSERT(m_pSidebandDevice != NULL);
        if (m_pSidebandDevice->IsNRECSupported())
        {
            modes = BthHfpMicPinSupportedDeviceModesNrec;
            numModes = ARRAYSIZE(BthHfpMicPinSupportedDeviceModesNrec);
        }
        else
        {
            modes = BthHfpMicPinSupportedDeviceModesNoNrec;
            numModes = ARRAYSIZE(BthHfpMicPinSupportedDeviceModesNoNrec);
        }
    }
#endif // SYSVAD_BTH_BYPASS

    if (ppModes != NULL)
    {
        if (numModes > 0)
        {
            *ppModes = modes;
        }
        else
        {
            // ensure that the returned pointer is NULL
            // in the event of failure (SAL annotation above
            // indicates that it must be NULL, and OACR sees a possibility
            // that it might not be).
            *ppModes = NULL;
        }
    }

    ReleaseFormatsAndModesLock();
    return numModes;
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsSystemCapturePin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == SystemCapturePin);
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsCellularBiDiCapturePin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == TelephonyBidiPin);
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsSystemRenderPin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == SystemRenderPin);
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsLoopbackPin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == RenderLoopbackPin);
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsOffloadPin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == OffloadRenderPin);
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsBridgePin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == BridgePin);
}

#pragma code_seg()
BOOL CMiniportWaveRT::IsKeywordDetectorPin(ULONG nPinId)
{
    AcquireFormatsAndModesLock();

    PINTYPE pinType = m_DeviceFormatsAndModes[nPinId].PinType;

    ReleaseFormatsAndModesLock();
    return (pinType == KeywordCapturePin);
}


//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::StreamCreated
(
    _In_ ULONG                  _Pin,
    _In_ PCMiniportWaveRTStream _Stream
)
{
    PAGED_CODE();

    PCMiniportWaveRTStream * streams        = NULL;
    ULONG                    count          = 0;
    
    DPF_ENTER(("[CMiniportWaveRT::StreamCreated]"));
    
    if (IsSystemCapturePin(_Pin) || IsCellularBiDiCapturePin(_Pin))
    {
        ALLOCATE_PIN_INSTANCE_RESOURCES(m_ulSystemAllocated);
        return STATUS_SUCCESS;
    }
    if (IsKeywordDetectorPin(_Pin))
    {
        ALLOCATE_PIN_INSTANCE_RESOURCES(m_ulKeywordDetectorAllocated);
        return STATUS_SUCCESS;
    }
    else if (IsLoopbackPin(_Pin))
    {
        ALLOCATE_PIN_INSTANCE_RESOURCES(m_ulLoopbackAllocated);
        streams = m_LoopbackStreams;
        count = m_ulMaxLoopbackStreams;
        _Stream->m_SaveData.Disable(m_MixDrmRights.CopyProtect);
    }
    else if (IsSystemRenderPin(_Pin))
    {
        ALLOCATE_PIN_INSTANCE_RESOURCES(m_ulSystemAllocated);
        streams = m_SystemStreams;
        count = m_ulMaxSystemStreams;
    }
    else if (IsOffloadPin(_Pin))
    {
        ALLOCATE_PIN_INSTANCE_RESOURCES(m_ulOffloadAllocated);
        streams = m_OffloadStreams;
        count = m_ulMaxOffloadStreams;
    }
    
    //
    // Cache this stream's ptr.
    //
    if (streams != NULL)
    {
        ULONG i = 0;
        for (; i<count; ++i)
        {
            if (streams[i] == NULL)
            {
                streams[i] = _Stream;
                break;
            }
        }
        ASSERT(i != count);
    }

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::StreamClosed
(
    _In_ ULONG                  _Pin,
    _In_ PCMiniportWaveRTStream _Stream
)
{
    PAGED_CODE();

    bool                      updateDrmRights = false;
    PCMiniportWaveRTStream  * streams         = NULL;
    ULONG                     count           = 0;

    DPF_ENTER(("[CMiniportWaveRT::StreamClosed]"));

    if (IsSystemCapturePin(_Pin) || IsCellularBiDiCapturePin(_Pin))
    {
        FREE_PIN_INSTANCE_RESOURCES(m_ulSystemAllocated);
        return STATUS_SUCCESS;
    }
    if (IsKeywordDetectorPin(_Pin))
    {
        FREE_PIN_INSTANCE_RESOURCES(m_ulKeywordDetectorAllocated);
        return STATUS_SUCCESS;
    }
    else if (IsLoopbackPin(_Pin))
    {
        FREE_PIN_INSTANCE_RESOURCES(m_ulLoopbackAllocated);
        streams = m_LoopbackStreams;
        count = m_ulMaxLoopbackStreams;
    }
    else if (IsSystemRenderPin(_Pin))
    {
        FREE_PIN_INSTANCE_RESOURCES(m_ulSystemAllocated);
        streams = m_SystemStreams;
        count = m_ulMaxSystemStreams;
        updateDrmRights = true;
    }
    else if (IsOffloadPin(_Pin))
    {
        FREE_PIN_INSTANCE_RESOURCES(m_ulOffloadAllocated);
        streams = m_OffloadStreams;
        count = m_ulMaxOffloadStreams;
        updateDrmRights = true;
    }

    //
    // Cleanup.
    //
    if (streams != NULL)
    {
        ULONG i = 0;
        for (; i<count; ++i)
        {
            if (streams[i] == _Stream)
            {
                streams[i] = NULL;
                break;
            }
        }
        ASSERT(i != count);
    }
    
    //
    // Update mixed drm rights.
    //
    if (updateDrmRights)
    {
        UpdateDrmRights();
    }

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::IsFormatSupported
(
    _In_ ULONG          _ulPin,
    _In_ BOOLEAN        _bCapture,
    _In_ PKSDATAFORMAT  _pDataFormat
)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::IsFormatSupported]"));

    NTSTATUS                            ntStatus = STATUS_NO_MATCH;
    PKSDATAFORMAT_WAVEFORMATEXTENSIBLE  pPinFormats = NULL;
    ULONG                               cPinFormats = 0;

    UNREFERENCED_PARAMETER(_bCapture);

    if (_ulPin >= m_pMiniportPair->WaveDescriptor->PinCount)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // For Lama Loopback, use the dynamic format.
    if (m_DeviceType == eLamaLoopbackRenderDevice || m_DeviceType == eLamaLoopbackCaptureDevice)
    {
        pPinFormats = &m_dynamicFormat16ch;
        cPinFormats = 1;
    }
    else
    {
        cPinFormats = GetPinSupportedDeviceFormats(_ulPin, &pPinFormats);
    }


    for (UINT iFormat = 0; iFormat < cPinFormats; iFormat++)
    {
        PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFormat = &pPinFormats[iFormat];
        // KSDATAFORMAT VALIDATION
        if (!IsEqualGUIDAligned(pFormat->DataFormat.MajorFormat, _pDataFormat->MajorFormat)) { continue; }
        if (!IsEqualGUIDAligned(pFormat->DataFormat.SubFormat, _pDataFormat->SubFormat)) { continue; }
        if (!IsEqualGUIDAligned(pFormat->DataFormat.Specifier, _pDataFormat->Specifier)) { continue; }
        if (pFormat->DataFormat.FormatSize < sizeof(KSDATAFORMAT_WAVEFORMATEX)) { continue; }

        // WAVEFORMATEX VALIDATION
        PWAVEFORMATEX pWaveFormat = reinterpret_cast<PWAVEFORMATEX>(_pDataFormat + 1);
        
        if (pWaveFormat->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
        {
            if (pWaveFormat->wFormatTag != EXTRACT_WAVEFORMATEX_ID(&(pFormat->WaveFormatExt.SubFormat))) { continue; }
        }
        if (pWaveFormat->nChannels  != pFormat->WaveFormatExt.Format.nChannels) { continue; }
        if (pWaveFormat->nSamplesPerSec != pFormat->WaveFormatExt.Format.nSamplesPerSec) { continue; }
        if (pWaveFormat->nBlockAlign != pFormat->WaveFormatExt.Format.nBlockAlign) { continue; }
        if (pWaveFormat->wBitsPerSample != pFormat->WaveFormatExt.Format.wBitsPerSample) { continue; }

        if (pWaveFormat->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
        {
            ntStatus = STATUS_SUCCESS;
            break;
        }

        // WAVEFORMATEXTENSIBLE VALIDATION
        if (pWaveFormat->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) { continue; }

        PWAVEFORMATEXTENSIBLE pWaveFormatExt = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pWaveFormat);
        if (pWaveFormatExt->Samples.wValidBitsPerSample != pFormat->WaveFormatExt.Samples.wValidBitsPerSample) { continue; }
        if (pWaveFormatExt->dwChannelMask != pFormat->WaveFormatExt.dwChannelMask) { continue; }
        if (!IsEqualGUIDAligned(pWaveFormatExt->SubFormat, pFormat->WaveFormatExt.SubFormat)) { continue; }

        ntStatus = STATUS_SUCCESS;
        break;
    }

    return ntStatus;
}

#ifdef SYSVAD_BTH_BYPASS
//=============================================================================
#pragma code_seg()
VOID
CMiniportWaveRT::EvtFormatChangeHandler
(
    _In_opt_    PVOID   Context
)
{
    DPF_ENTER(("[CMiniportWaveRT::EvtFormatChangeHandler]"));

    PCMiniportWaveRT This = PCMiniportWaveRT(Context);
    if (This == NULL)
    {
        DPF(D_ERROR, ("EvtFormatChangeHandler: context is null")); 
        return;
    }

    /*
    _In_opt_    GUID   *Set,
    _In_        ULONG   EventId,
    _In_        BOOL    PinEvent,
    _In_        ULONG   PinId,
    _In_        BOOL    NodeEvent,
    _In_        ULONG   NodeId
    */

    if (This->IsSidebandDevice())
    {
        if (This->m_DeviceType == eBthHfpMicDevice)
        {
            // swap the device formats and modes for bt
            This->AcquireFormatsAndModesLock();

            This->m_DeviceFormatsAndModes = This->m_pSidebandDevice->GetFormatsAndModes(This->m_DeviceType);

            This->ReleaseFormatsAndModesLock();

            This->GenerateEventList(
                (GUID*)&KSEVENTSETID_PinCapsChange,
                KSEVENT_PINCAPS_FORMATCHANGE,
                TRUE,
                1,
                FALSE,
                ULONG(-1));
        }
        else if(This->m_DeviceType == eBthHfpSpeakerDevice)
        {
            // swap the device formats and modes for bt
            This->AcquireFormatsAndModesLock();

            This->m_DeviceFormatsAndModes = This->m_pSidebandDevice->GetFormatsAndModes(This->m_DeviceType);

            This->ReleaseFormatsAndModesLock();

            This->GenerateEventList(
                (GUID*)&KSEVENTSETID_PinCapsChange,
                KSEVENT_PINCAPS_FORMATCHANGE,
                TRUE,
                0,
                FALSE,
                ULONG(-1));
        }
    }
}
#endif  // #ifdef SYSVAD_BTH_BYPASS

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerProposedFormat
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PKSP_PIN                kspPin                  = NULL;
    PKSDATAFORMAT           pKsFormat               = NULL;
    ULONG                   cbMinSize               = 0;
    NTSTATUS                ntStatus                = STATUS_INVALID_PARAMETER;

    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerProposedFormat]"));
    
    // All properties handled by this handler require at least a KSP_PIN descriptor.

    // Verify instance data stores at least KSP_PIN fields beyond KSPPROPERTY.
    if (PropertyRequest->InstanceSize < (sizeof(KSP_PIN) - RTL_SIZEOF_THROUGH_FIELD(KSP_PIN, Property)))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Extract property descriptor from property request instance data
    kspPin = CONTAINING_RECORD(PropertyRequest->Instance, KSP_PIN, PinId);

    //
    // This method is valid only on streaming pins.
    //
    if (IsSystemRenderPin(kspPin->PinId) ||
        IsLoopbackPin(kspPin->PinId) ||
        IsOffloadPin(kspPin->PinId) ||
        IsSystemCapturePin(kspPin->PinId) ||
        IsKeywordDetectorPin(kspPin->PinId) ||
        IsCellularBiDiCapturePin(kspPin->PinId))
    {
        ntStatus = STATUS_SUCCESS;
    }
    else if (IsBridgePin(kspPin->PinId))
    {
        ntStatus = STATUS_NOT_SUPPORTED;
    }
    else 
    {
        ntStatus = STATUS_INVALID_PARAMETER;
    }

    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    cbMinSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
   
    // Handle KSPROPERTY_TYPE_BASICSUPPORT query
    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ULONG flags = PropertyRequest->PropertyItem->Flags;
        
        return PropertyHandler_BasicSupport(PropertyRequest, flags, VT_ILLEGAL);
    }

    // Verify value size
    if (PropertyRequest->ValueSize == 0)
    {
        PropertyRequest->ValueSize = cbMinSize;
        return STATUS_BUFFER_OVERFLOW;
    }
    if (PropertyRequest->ValueSize < cbMinSize)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

#if 0
    // Only SET is supported for this property
    if ((PropertyRequest->Verb & KSPROPERTY_TYPE_SET) == 0)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
#endif

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
#if defined(SYSVAD_BTH_BYPASS) || defined(SYSVAD_USB_SIDEBAND)
        if (IsSidebandDevice())
        {
            if (m_DeviceType == eBthHfpMicDevice ||
                m_DeviceType == eBthHfpSpeakerDevice ||
                m_DeviceType == eUsbHsSpeakerDevice ||
                m_DeviceType == eUsbHsMicDevice)
            {
                KSDATAFORMAT_WAVEFORMATEXTENSIBLE *propFormat = (KSDATAFORMAT_WAVEFORMATEXTENSIBLE *)PropertyRequest->Value;
                ULONG numModes = 0;
                MODE_AND_DEFAULT_FORMAT *modeInfo = NULL;
                MODE_AND_DEFAULT_FORMAT *modeInfo_RAW = NULL;
                numModes = GetPinSupportedDeviceModes(kspPin->PinId, &modeInfo);
                BOOL bFound = FALSE;
                ULONG i = 0;

                // For loopback pin, get default format from host pin structures
                if (IsLoopbackPin(kspPin->PinId))
                {
                    for (i = 0; i < m_DeviceFormatsAndModesCount; i++)
                    {
                        if (m_DeviceFormatsAndModes[i].PinType == SystemRenderPin)
                        {
                            modeInfo = m_DeviceFormatsAndModes[i].ModeAndDefaultFormat;
                            numModes = m_DeviceFormatsAndModes[i].ModeAndDefaultFormatCount;
                            break;
                        }
                    }
                }

                // Iterate through FormatsAndModes to find the 'DefaultFormat' for the 'DEFAULT' processing mode
                // Make note of the RAW format for cases where DEFAULT mode is not supported by endpoint
                for (i = 0; i < numModes; i++, ++modeInfo)
                {
                    if ((IsEqualGUIDAligned(modeInfo->Mode, AUDIO_SIGNALPROCESSINGMODE_DEFAULT)) &&
                        (modeInfo->DefaultFormat != NULL))
                    {
                        bFound = TRUE;
                        break;
                    }
                    else if ((IsEqualGUIDAligned(modeInfo->Mode, AUDIO_SIGNALPROCESSINGMODE_RAW)) &&
                        (modeInfo->DefaultFormat != NULL))
                    {
                        modeInfo_RAW = modeInfo;
                    }
                }

                if (!bFound &&
                    modeInfo_RAW)
                {
                    modeInfo = modeInfo_RAW;
                    bFound = TRUE;
                }

                if (!bFound)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                RtlCopyMemory(propFormat, modeInfo->DefaultFormat, modeInfo->DefaultFormat->FormatSize);
                PropertyRequest->ValueSize = modeInfo->DefaultFormat->FormatSize;
                ntStatus = STATUS_SUCCESS;
            }
        }
        else
#endif // defined(SYSVAD_BTH_BYPASS) || defined(SYSVAD_USB_SIDEBAND)
        if (IsKeywordDetectorPin(kspPin->PinId))
        {
            KSDATAFORMAT_WAVEFORMATEXTENSIBLE *propFormat = (KSDATAFORMAT_WAVEFORMATEXTENSIBLE *)PropertyRequest->Value;
            ULONG numModes = 0;
            MODE_AND_DEFAULT_FORMAT *modeInfo = NULL;

            numModes = GetPinSupportedDeviceModes(kspPin->PinId, &modeInfo);
            if (numModes == 0 || modeInfo->DefaultFormat == NULL)
            {
                return STATUS_NOT_SUPPORTED;
            }

            RtlCopyMemory(propFormat, modeInfo->DefaultFormat, modeInfo->DefaultFormat->FormatSize);
            PropertyRequest->ValueSize = modeInfo->DefaultFormat->FormatSize;
            ntStatus = STATUS_SUCCESS;
        }
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
    {
        pKsFormat = (PKSDATAFORMAT)PropertyRequest->Value;
        ntStatus = IsFormatSupported(kspPin->PinId,
            IsSystemCapturePin(kspPin->PinId) || IsCellularBiDiCapturePin(kspPin->PinId) ||
            IsLoopbackPin(kspPin->PinId),
            pKsFormat);
        if (!NT_SUCCESS(ntStatus))
        {
            return ntStatus;
        }

        //
        // Make sure there are enough resources to handle a new pin creation with
        // this format.
        //
        if (IsOffloadPin(kspPin->PinId))
        {
            ntStatus = ValidateStreamCreate(kspPin->PinId, FALSE);
        }
    }

    return ntStatus;
} // PropertyHandlerProposedFormat

//=============================================================================
#pragma code_seg()
VOID
CMiniportWaveRT::GenerateEventList
(
    _In_opt_    GUID   *Set,
    _In_        ULONG   EventId,
    _In_        BOOL    PinEvent,
    _In_        ULONG   PinId,
    _In_        BOOL    NodeEvent,
    _In_        ULONG   NodeId
)
/*++

Routine Description:

  The GenerateEventList method notifies clients through the port driver's list 
  of event entries that a particular event has occurred.

Arguments:

  Set -

  EventId - 

  PinEvent -

  PinId -

  NodeEvent -

  NodeId -

--*/
{
    DPF_ENTER(("[CMiniportWaveRT::GenerateEventList]"));

    ASSERT(m_pPortEvents != NULL);

    m_pPortEvents->GenerateEventList(
        Set,
        EventId,
        PinEvent,
        PinId,
        NodeEvent,
        NodeId);
}

//=============================================================================
#pragma code_seg()
VOID
CMiniportWaveRT::SendPNPNotification(
    _In_ const GUID *                   NotificationId,
    _In_ PVOID                          NotificationBuffer, 
    _In_ USHORT                         NotificationBufferCb
    )
{
    NTSTATUS                status = STATUS_SUCCESS;
    PPCNOTIFICATION_BUFFER  buffer = NULL;

    DPF_ENTER(("[SendPNPNotification]"));

    // Allocate a notification buffer.
    status = m_pPortClsNotifications->AllocNotificationBuffer(PagedPool,
                                                        NotificationBufferCb,
                                                        &buffer);
    if (!NT_SUCCESS(status))
    {
        goto exit;
    }

    // Notification buffer is only guaranteed to be LONG aligned,
    // it is received as ULONGLONG aligned on the receiving end.
    RtlCopyMemory(buffer, NotificationBuffer, NotificationBufferCb);    

    //
    // Generate notification (async).
    //
    m_pPortClsNotifications->SendNotification(NotificationId, buffer);

exit:
    if (buffer != NULL)
    {
        // Free notification buffer.
        m_pPortClsNotifications->FreeNotificationBuffer(buffer);
        buffer = NULL;
    }
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerAudioEffectsDiscoveryEffectsList
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerAudioEffectsDiscoveryEffectsList]"));

    PKSP_PIN                kspPin                  = NULL;
    NTSTATUS                ntStatus                = STATUS_INVALID_PARAMETER;
    PKSP_PINMODE            pKspPinmode             = NULL;
    GUID                    signalProcessingMode    = AUDIO_SIGNALPROCESSINGMODE_DEFAULT;

    // Verify instance data stores at least KSP_PIN fields beyond KSPPROPERTY.
    if (PropertyRequest->InstanceSize < (sizeof(KSP_PIN) - RTL_SIZEOF_THROUGH_FIELD(KSP_PIN, Property)))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Extract property descriptor from property request instance data
    kspPin = CONTAINING_RECORD(PropertyRequest->Instance, KSP_PIN, PinId);

    // Get the mode if specified.
    pKspPinmode = (PKSP_PINMODE)kspPin;
    signalProcessingMode = pKspPinmode->AudioProcessingMode;

    if (PropertyRequest->InstanceSize >= sizeof(ULONG))
    {
        // This prop is only supported on Keyword Detector pins.
        if (IsKeywordDetectorPin(kspPin->PinId))
        {
            ntStatus = STATUS_SUCCESS;
        }
        else
        {
            ntStatus = STATUS_NOT_SUPPORTED;
        }
    }

    IF_FAILED_JUMP(ntStatus, Done);

    // Valid actions: get and basicsupport.
    ntStatus = STATUS_INVALID_PARAMETER;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = 
            PropertyHandler_BasicSupport
            (
                PropertyRequest,
                KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET,
                VT_ILLEGAL
            );
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        if (IsKeywordDetectorPin(kspPin->PinId))
        {
            // Compute total size, two effects: NS and EC (see below).
            ULONG cbMinSize = 0;

            if(signalProcessingMode == AUDIO_SIGNALPROCESSINGMODE_SPEECH)
            {
                cbMinSize = sizeof(GUID) * EFFECTS_LIST_COUNT;
            }
            
            if (PropertyRequest->ValueSize == 0)
            {
                PropertyRequest->ValueSize = cbMinSize;
                ntStatus = STATUS_BUFFER_OVERFLOW;
            }
            else if (PropertyRequest->ValueSize < cbMinSize)
            {
                ntStatus = STATUS_BUFFER_TOO_SMALL;
            }
            else if (signalProcessingMode == AUDIO_SIGNALPROCESSINGMODE_SPEECH)
            {
                PGUID effectList = PGUID(PropertyRequest->Value);

                *effectList = AUDIO_EFFECT_TYPE_ACOUSTIC_ECHO_CANCELLATION;
                *(effectList + 1) = AUDIO_EFFECT_TYPE_NOISE_SUPPRESSION;
                
                PropertyRequest->ValueSize = cbMinSize;
                ntStatus = STATUS_SUCCESS;
            } 
            else 
            {
                PropertyRequest->ValueSize = 0;
                ntStatus = STATUS_SUCCESS;
            }
        }
    }

Done:

    return ntStatus;
} // PropertyHandlerAudioEffectsDiscoveryEffectsList

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerModulesListRequest
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    // This specific APO->driver communication example is mainly added to show to this communication is done.
    // The module list only lives on the wave filter and it can have modules that are for all pins and some that 
    // are only on specific pins.

    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerModulesListRequest]"));

    return AudioModule_GenericHandler_ModulesListRequest(
                PropertyRequest,
                GetAudioModuleList(),
                GetAudioModuleListCount());
} // PropertyHandlerModulesListRequest

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerModuleCommand
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerModuleCommand]"));

    return AudioModule_GenericHandler_ModuleCommand(
                PropertyRequest,
                GetAudioModuleList(),
                GetAudioModuleListCount());
} // PropertyHandlerModuleCommand

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerModuleNotificationDeviceId
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerModuleNotificationDeviceId]"));

    return AudioModule_GenericHandler_ModuleNotificationDeviceId(
                PropertyRequest,
                GetAudioModuleNotificationDeviceId());
} // PropertyHandlerModuleNotificationDeviceId

//=============================================================================
#pragma code_seg()
NTSTATUS
CMiniportWaveRT_EventHandler_PinCapsChange
(
_In_  PPCEVENT_REQUEST EventRequest
)
{
    CMiniportWaveRT* miniport = reinterpret_cast<CMiniportWaveRT*>(EventRequest->MajorTarget);
    return miniport->EventHandler_PinCapsChange(EventRequest);
}

//=============================================================================
#pragma code_seg()
NTSTATUS
CMiniportWaveRT::EventHandler_PinCapsChange
(
_In_  PPCEVENT_REQUEST EventRequest
)
{
    if (*EventRequest->EventItem->Set != KSEVENTSETID_PinCapsChange)
    {
        return STATUS_INVALID_PARAMETER;
    }

    switch (EventRequest->Verb)
    {
        // Do we support event handling?!?
    case PCEVENT_VERB_SUPPORT:
        break;
        // We should add the event now!
    case PCEVENT_VERB_ADD:
        // If we have the interface and EventEntry is defined ...
        if (EventRequest->EventEntry)
        {
            switch (EventRequest->EventItem->Id)
            {
                // Add pincaps format change event to support the force sample rate feature
                case KSEVENT_PINCAPS_FORMATCHANGE:
                    m_pPortEvents->AddEventToEventList(EventRequest->EventEntry);
                    break;
                default:
                    return STATUS_INVALID_PARAMETER;
                    break;
            }
        }
        else
        {
            return STATUS_UNSUCCESSFUL;
        }
        break;

    case PCEVENT_VERB_REMOVE:
        // We cannot remove the event but we can stop generating the
        // events. However, it also doesn't hurt to always generate them ...
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}


//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerProposedFormat2
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PKSP_PIN                kspPin                  = NULL;
    ULONG                   cbMinSize               = 0;
    NTSTATUS                ntStatus                = STATUS_INVALID_PARAMETER;
    ULONG                   numModes                = 0;
    MODE_AND_DEFAULT_FORMAT *modeInfo               = NULL;
    MODE_AND_DEFAULT_FORMAT *modeTemp               = NULL;
    PKSMULTIPLE_ITEM        pKsItemsHeader          = NULL;
    PKSMULTIPLE_ITEM        pKsItemsHeaderOut       = NULL;
    size_t                  cbItemsList             = 0;
    GUID                    signalProcessingMode    = {0};
    BOOLEAN                 bFound                  = FALSE;
    ULONG                   i;

    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerProposedFormat2]"));
    
    // All properties handled by this handler require at least a KSP_PIN descriptor.

    // Verify instance data stores at least KSP_PIN fields beyond KSPPROPERTY.
    if (PropertyRequest->InstanceSize < (sizeof(KSP_PIN) - RTL_SIZEOF_THROUGH_FIELD(KSP_PIN, Property)))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Extract property descriptor from property request instance data
    kspPin = CONTAINING_RECORD(PropertyRequest->Instance, KSP_PIN, PinId);

    if (kspPin->PinId >= m_pMiniportPair->WaveDescriptor->PinCount)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // This property is supported only on some streaming pins.
    //
    numModes = GetPinSupportedDeviceModes(kspPin->PinId, &modeInfo);

    ASSERT((modeInfo != NULL && numModes > 0) || (modeInfo == NULL && numModes == 0));

    if (modeInfo == NULL)
    {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Even for pins that support modes, the pin might not support proposed formats
    //
    bFound = FALSE;
    for (i=0, modeTemp=modeInfo; i<numModes; ++i, ++modeTemp)
    {
        if (modeTemp->DefaultFormat != NULL)
        {
            bFound = TRUE;
            break;
        }
    }

    if (!bFound)
    {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // The property is generally supported on this pin. Handle basic support request.
    //
    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        return PropertyHandler_BasicSupport(PropertyRequest, PropertyRequest->PropertyItem->Flags, VT_ILLEGAL);
    }

    //
    // Get the mode if specified.
    //
    pKsItemsHeader = (PKSMULTIPLE_ITEM)(kspPin + 1);
    cbItemsList = (((PBYTE)PropertyRequest->Instance) + PropertyRequest->InstanceSize) - (PBYTE)pKsItemsHeader;

    ntStatus = GetAttributesFromAttributeList(pKsItemsHeader, cbItemsList, &signalProcessingMode);
    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    //
    // Get the info associated with this mode.
    //
    bFound = FALSE;
    for (i=0; i<numModes; ++i, ++modeInfo)
    {
        if (modeInfo->Mode == signalProcessingMode)
        {
            bFound = TRUE;
            break;
        }
    }

    // Either the mode isn't supported, or the driver doesn't support a
    // proprosed format for this specific mode.
    if (!bFound || modeInfo->DefaultFormat == NULL)
    {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Compute output data buffer.
    //
    if (m_DeviceType == eLamaLoopbackRenderDevice || m_DeviceType == eLamaLoopbackCaptureDevice)
    {
        cbMinSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE); // Size of our dynamic format
    }
    else
    {
        cbMinSize = modeInfo->DefaultFormat->FormatSize;
    }
    cbMinSize = (cbMinSize + 7) & ~7;

    pKsItemsHeaderOut = (PKSMULTIPLE_ITEM)((PBYTE)PropertyRequest->Value + cbMinSize);

    if (cbItemsList > MAXULONG)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Total # of bytes.
    ntStatus = RtlULongAdd(cbMinSize, (ULONG)cbItemsList, &cbMinSize);
    if (!NT_SUCCESS(ntStatus))
    {
        return STATUS_INVALID_PARAMETER;
    }
        
    // Property not supported.
    if (cbMinSize == 0)
    {
        return STATUS_NOT_SUPPORTED;
    }

    // Verify value size
    if (PropertyRequest->ValueSize == 0)
    {
        PropertyRequest->ValueSize = cbMinSize;
        return STATUS_BUFFER_OVERFLOW;
    }
    if (PropertyRequest->ValueSize < cbMinSize)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Only GET is supported for this property
    if ((PropertyRequest->Verb & KSPROPERTY_TYPE_GET) == 0)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    // Copy the proposed default format.
    if (m_DeviceType == eLamaLoopbackRenderDevice || m_DeviceType == eLamaLoopbackCaptureDevice)
    {
        RtlCopyMemory(PropertyRequest->Value, &m_dynamicFormat16ch, sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE));
        ((PKSDATAFORMAT)PropertyRequest->Value)->FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    }
    else
    {
        RtlCopyMemory(PropertyRequest->Value, modeInfo->DefaultFormat, modeInfo->DefaultFormat->FormatSize);
    }


    // Copy back the attribute list.
    ASSERT(cbItemsList > 0);
    ((KSDATAFORMAT*)PropertyRequest->Value)->Flags = KSDATAFORMAT_ATTRIBUTES;
    RtlCopyMemory(pKsItemsHeaderOut, pKsItemsHeader, cbItemsList);
    
    PropertyRequest->ValueSize = cbMinSize;

    return STATUS_SUCCESS;
} // PropertyHandlerProposedFormat


//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandler_WaveFilter // Renamed from PropertyHandler_WaveFilterSysVAD
( 
    _In_ PPCPROPERTY_REQUEST      PropertyRequest 
)
/*++

Routine Description:

  Redirects general property request to miniport object

Arguments:

  PropertyRequest - 

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    CMiniportWaveRT*    pWaveHelper = reinterpret_cast<CMiniportWaveRT*>(PropertyRequest->MajorTarget);

    if (pWaveHelper == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    pWaveHelper->AddRef();

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_SysVAD))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
            case KSPROPERTY_SYSVAD_DEFAULTSTREAMEFFECTS:
                ntStatus = pWaveHelper->PropertyHandlerEffectListRequest(PropertyRequest);
                break;
            default:
                DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
        }
    }
    else if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Pin))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
            case KSPROPERTY_PIN_PROPOSEDATAFORMAT:
                ntStatus = pWaveHelper->PropertyHandlerProposedFormat(PropertyRequest);
                break;
            
            case KSPROPERTY_PIN_PROPOSEDATAFORMAT2:
                ntStatus = pWaveHelper->PropertyHandlerProposedFormat2(PropertyRequest);
                break;

            default:
                DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
        }
    }
    else if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_AudioModule))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
        case KSPROPERTY_AUDIOMODULE_DESCRIPTORS:
            ntStatus = pWaveHelper->PropertyHandlerModulesListRequest(PropertyRequest);
            break;
        case KSPROPERTY_AUDIOMODULE_COMMAND:
            ntStatus = pWaveHelper->PropertyHandlerModuleCommand(PropertyRequest);
            break;
        case KSPROPERTY_AUDIOMODULE_NOTIFICATION_DEVICE_ID:
            ntStatus = pWaveHelper->PropertyHandlerModuleNotificationDeviceId(PropertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
        }
    }
    else if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_AudioEffectsDiscovery))
    {
        switch(PropertyRequest->PropertyItem->Id)
        {
        case KSPROPERTY_AUDIOEFFECTSDISCOVERY_EFFECTSLIST:
            ntStatus = pWaveHelper->PropertyHandlerAudioEffectsDiscoveryEffectsList(PropertyRequest);
            break;
            
        default:
            DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
        }
    }
    else if ((pWaveHelper->m_DeviceType == eHdmiRenderDevice || 
              pWaveHelper->m_DeviceType == eCellularDevice || 
              pWaveHelper->m_DeviceType == eHandsetSpeakerDevice) &&
             IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Audio))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
            case KSPROPERTY_AUDIO_VOLUMELEVEL:
                ntStatus = PropertyHandler_Volume(
                                    pWaveHelper->m_pAdapterCommon,
                                    PropertyRequest,
                                    pWaveHelper->m_DeviceMaxChannels);
                break;
            
            case KSPROPERTY_AUDIO_MUTE:
                ntStatus = PropertyHandler_Mute(
                                    pWaveHelper->m_pAdapterCommon,
                                    PropertyRequest,
                                    pWaveHelper->m_DeviceMaxChannels);
                break;
            
            case KSPROPERTY_AUDIO_PEAKMETER2:
                ntStatus = PropertyHandler_PeakMeter2(
                                    pWaveHelper->m_pAdapterCommon,
                                    PropertyRequest,
                                    pWaveHelper->m_DeviceMaxChannels);
                break;
            
            case KSPROPERTY_AUDIO_CPU_RESOURCES:
                ntStatus = PropertyHandler_CpuResources(PropertyRequest);
                break;
            
            default:
                DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
                
        }
    }
    else if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_LamaLoopback) &&
             (pWaveHelper->m_DeviceType == eLamaLoopbackRenderDevice || pWaveHelper->m_DeviceType == eLamaLoopbackCaptureDevice))
    {
        ntStatus = CMiniportWaveRT::PropertyHandlerLamaSampleRate(PropertyRequest);
    }


    pWaveHelper->Release();

    return ntStatus;
} // PropertyHandler_WaveFilter

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
CMiniportWaveRT::PropertyHandlerLamaSampleRate
(
    _In_ PPCPROPERTY_REQUEST PropertyRequest
)
{
    PAGED_CODE();
    ASSERT(PropertyRequest);
    DPF_ENTER(("[CMiniportWaveRT::PropertyHandlerLamaSampleRate]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    CMiniportWaveRT* pMiniport = reinterpret_cast<CMiniportWaveRT*>(PropertyRequest->MajorTarget);

    if (pMiniport == NULL || 
        !(pMiniport->m_DeviceType == eLamaLoopbackRenderDevice || pMiniport->m_DeviceType == eLamaLoopbackCaptureDevice))
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (PropertyRequest->PropertyItem->Id != KSPROPERTY_LAMA_SAMPLE_RATE)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ULONG flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, flags, VT_UI4);
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        if (PropertyRequest->ValueSize >= sizeof(ULONG))
        {
            *(PULONG(PropertyRequest->Value)) = pMiniport->m_currentSampleRate;
            PropertyRequest->ValueSize = sizeof(ULONG);
            ntStatus = STATUS_SUCCESS;
        }
        else if (PropertyRequest->ValueSize == 0) // Query for size
        {
             PropertyRequest->ValueSize = sizeof(ULONG);
             ntStatus = STATUS_BUFFER_OVERFLOW;
        }
        else // Buffer too small
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
    {
        if (PropertyRequest->ValueSize >= sizeof(ULONG))
        {
            ULONG newSampleRate = *(PULONG(PropertyRequest->Value));
            // Basic validation - a real driver might have a list of supported rates.
            if (newSampleRate >= 8000 && newSampleRate <= 192000) 
            {
                pMiniport->m_currentSampleRate = newSampleRate;
                
                // Update the dynamic format
                pMiniport->m_dynamicFormat16ch.WaveFormatEx.nSamplesPerSec = newSampleRate;
                pMiniport->m_dynamicFormat16ch.WaveFormatEx.nAvgBytesPerSec = 
                    newSampleRate * 
                    pMiniport->m_dynamicFormat16ch.WaveFormatEx.nChannels * 
                    (pMiniport->m_dynamicFormat16ch.WaveFormatEx.wBitsPerSample / 8);
                
                // NOTE: This is where KSEVENT_PINCAPS_FORMATCHANGE would ideally be signaled
                // to notify clients of the format change. This part is deferred.

                ntStatus = STATUS_SUCCESS;
            }
            else
            {
                ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
        else
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
    }
    
    return ntStatus;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
PropertyHandler_OffloadPin
( 
    _In_ PPCPROPERTY_REQUEST      PropertyRequest 
)
/*++

Routine Description:

  Redirects general property request to miniport object

Arguments:

  PropertyRequest - 

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_OffloadPin))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
            //KSPROPERTY_OFFLOAD_PIN_VERIFY_STREAM_OBJECT_POINTER

            case KSPROPERTY_OFFLOAD_PIN_GET_STREAM_OBJECT_POINTER:
            {
                if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
                {
                    ULONG cbMinSize = sizeof(ULONG_PTR);

                    if (PropertyRequest->ValueSize == 0)
                    {
                        PropertyRequest->ValueSize = cbMinSize;
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    }
                    else if (PropertyRequest->ValueSize < cbMinSize)
                    {
                        ntStatus = STATUS_BUFFER_TOO_SMALL;
                    }
                    else
                    {
                        ULONG_PTR *streamObjectPtr = static_cast<ULONG_PTR*>(PropertyRequest->Value);
                        *streamObjectPtr = (ULONG_PTR)(PropertyRequest->MinorTarget);
                        ntStatus = STATUS_SUCCESS;
                    }
                }

                else
                {
                    ntStatus = STATUS_INVALID_PARAMETER;
                }
            }
            break;
            case KSPROPERTY_OFFLOAD_PIN_VERIFY_STREAM_OBJECT_POINTER:
            {
                if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
                {
                    ULONG cbMinSize = sizeof(ULONG_PTR);
                    if (PropertyRequest->InstanceSize < cbMinSize)
                    {
                        return STATUS_INVALID_PARAMETER;
                    }
                    else
                    {
                        ULONG_PTR *streamObjectPtr = static_cast<ULONG_PTR*>(PropertyRequest->Instance);
                        if (*streamObjectPtr == (ULONG_PTR)(PropertyRequest->MinorTarget))
                        {
                            ntStatus = STATUS_SUCCESS;
                        }
                        else
                        {
                            ntStatus = STATUS_UNSUCCESSFUL;
                        }
                    }
                }
                else
                {
                    ntStatus = STATUS_INVALID_PARAMETER;
                }
            }
            break;
            default:
                DPF(D_TERSE, ("[PropertyHandler_OffloadPin: Invalid Request]"));
        }
    }
    return ntStatus;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
PropertyHandler_GenericPin
( 
    _In_ PPCPROPERTY_REQUEST      PropertyRequest 
)
{
    NTSTATUS                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    CMiniportWaveRT*        pWave = NULL;
    CMiniportWaveRTStream * pStream = NULL;

    PAGED_CODE();

    if (PropertyRequest->MajorTarget == NULL ||
        PropertyRequest->MinorTarget == NULL)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    
    //
    // Get a ref to the miniport.
    //
    pWave = MajorTarget_to_Obj(PropertyRequest->MajorTarget);
    pWave->AddRef();

    //
    // Get a ref to the stream.
    //
    pStream = MinorTarget_to_Obj(PropertyRequest->MinorTarget);
    pStream->AddRef();

    //
    // Invoke appropriate handle.
    //
    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_AudioModule))
    {
        switch (PropertyRequest->PropertyItem->Id)
        {
        case KSPROPERTY_AUDIOMODULE_DESCRIPTORS:
            ntStatus = pStream->PropertyHandlerModulesListRequest(PropertyRequest);
            break;
        case KSPROPERTY_AUDIOMODULE_COMMAND:
            ntStatus = pStream->PropertyHandlerModuleCommand(PropertyRequest);
            break;
        case KSPROPERTY_AUDIOMODULE_NOTIFICATION_DEVICE_ID:
            // Filter handles this prop.
            ntStatus = pWave->PropertyHandlerModuleNotificationDeviceId(PropertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandler_GenericPin: Invalid Device Request]"));
        }
    }

exit:

    SAFE_RELEASE(pStream);
    SAFE_RELEASE(pWave);
    
    return ntStatus;
}

// ISSUE-2014/10/20 Add synchronization mechanism throughout this class
// ISSUE-2014/10/20 Add comment headers and commenting throughout
#pragma code_seg("PAGE")
CKeywordDetector::CKeywordDetector()
    :
    m_streamRunning(FALSE),
    m_qpcStartCapture(0),
    m_nLastQueuedPacket(-1),
    m_SoundDetectorArmed1(FALSE),
    m_SoundDetectorArmed2(FALSE),
    m_SoundDetectorData1(0),
    m_SoundDetectorData2(0),
    m_ullKeywordStartTimestamp(0),
    m_ullKeywordStopTimestamp(0)
{
    PAGED_CODE();

    // Initialize our pool of packets and the list structures
    KeInitializeSpinLock(&PacketPoolSpinLock);
    KeInitializeSpinLock(&PacketFifoSpinLock);
    ResetFifo();
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::ReadKeywordTimestampRegistry()
{
    PAGED_CODE();

    NTSTATUS                    ntStatus;
    PDRIVER_OBJECT              DriverObject;
    HANDLE                      DriverKey;

    RTL_QUERY_REGISTRY_TABLE    paramTable[] = {
        // QueryRoutine     Flags                                               Name                            EntryContext                            DefaultType                                                     DefaultData                                 DefaultLength
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"KeywordDetectorStartTimestamp",   &m_ullKeywordStartTimestamp,    (REG_QWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_QWORD,  &m_ullKeywordStartTimestamp,        sizeof(ULONGLONG) },
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"KeywordDetectorStopTimestamp",    &m_ullKeywordStopTimestamp,     (REG_QWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_QWORD,  &m_ullKeywordStopTimestamp,         sizeof(ULONGLONG) },
        { NULL,   0,                                                        NULL,                               NULL,                                   0,                                                              NULL,                                       0 }
    };

    DriverObject = WdfDriverWdmGetDriverObject(WdfGetDriver());
    DriverKey = NULL;
    ntStatus = IoOpenDriverRegistryKey(DriverObject, 
                                 DriverRegKeyParameters,
                                 KEY_READ,
                                 0,
                                 &DriverKey);

    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    ntStatus = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                  (PCWSTR) DriverKey,
                                  &paramTable[0],
                                  NULL,
                                  NULL);

    if (!NT_SUCCESS(ntStatus)) 
    {
        DPF(D_VERBOSE, ("RtlQueryRegistryValues failed, using default values, 0x%x", ntStatus));
        //
        // Don't return error because we will operate with default values.
        //
    }

    if (DriverKey)
    {
        ZwClose(DriverKey);
    }

    return ntStatus;
}


#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::ResetDetector(_In_ GUID eventId)
{
    PAGED_CODE();

    if (eventId == CONTOSO_KEYWORD1)
    {
        m_SoundDetectorData1 = 0;
        m_SoundDetectorArmed1 = FALSE;
    }
    else if(eventId == CONTOSO_KEYWORD2)
    {
        m_SoundDetectorData2 = 0;
        m_SoundDetectorArmed2 = FALSE;
    }
    else if(eventId == GUID_NULL)
    {
        // When DownloadDetectorData is called to set the pattern for multiple keywords
        // at once, all keyword detectors must be reset. Also used during keyword detector
        // initialization and cleanup to restore it back to initial state and power down.
        m_SoundDetectorData1 = 0;
        m_SoundDetectorArmed1 = FALSE;
        m_SoundDetectorData2 = 0;
        m_SoundDetectorArmed2 = FALSE;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::DownloadDetectorData(_In_ GUID eventId, _In_ LONGLONG Data)
{
    PAGED_CODE();

    // reset the detector for this event Id
    ResetDetector(eventId);

    // In this example, the driver supports detection data 
    // set with a single call for both detectors, or each
    // detector set individually.
    if (eventId == CONTOSO_KEYWORD1)
    {
        m_SoundDetectorData1 = Data;
    }
    else if(eventId == CONTOSO_KEYWORD2)
    {
        m_SoundDetectorData2 = Data;
    }
    else if(eventId == GUID_NULL)
    {
        // in this simplified example "Data" is set on both detectors,
        // however in a real system "Data" could be a data structure which
        // contains different values for each detector.
        m_SoundDetectorData1 = m_SoundDetectorData2 = Data;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

// The following function is only applicable to single keyword detection systems,
// and assumes keyword detector #1.
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::GetDetectorData(_In_ GUID eventId, _Out_ LONGLONG *Data)
{
    PAGED_CODE();

    if (eventId == CONTOSO_KEYWORD1)
    {
        *Data = m_SoundDetectorData1;
    }
    else if(eventId == CONTOSO_KEYWORD2)
    {
        *Data = m_SoundDetectorData2;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }


    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONGLONG CKeywordDetector::GetStartTimestamp()
{
    PAGED_CODE();

    return m_ullKeywordStartTimestamp;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONGLONG CKeywordDetector::GetStopTimestamp()
{
    PAGED_CODE();

    return m_ullKeywordStopTimestamp;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID CKeywordDetector::ResetFifo()
{
    PAGED_CODE();

    m_qpcStartCapture = 0;
    m_nLastQueuedPacket = (-1);
    InitializeListHead(&PacketPoolHead);
    InitializeListHead(&PacketFifoHead);

    for (int i = 0; i < ARRAYSIZE(PacketPool); i++)
    {
        InsertTailList(&PacketPoolHead, &PacketPool[i].ListEntry);
    }
    return;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::SetArmed(_In_ GUID eventId, _In_ BOOL Arm)
{
    PAGED_CODE();

    BOOL previousArming = FALSE;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    // the previous state is "armed" if either detector is armed.
    // this reflects the fact that both detectors are sharing the
    // same stream.
    previousArming = m_SoundDetectorArmed1 || m_SoundDetectorArmed2;

    if (eventId == CONTOSO_KEYWORD1)
    {
        m_SoundDetectorArmed1 = Arm;
    }
    else if(eventId == CONTOSO_KEYWORD2)
    {
        m_SoundDetectorArmed2 = Arm;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Arm && !previousArming && m_qpcStartCapture == 0)
    {
        StartBufferingStream();
    }
    else if (!Arm && previousArming && !m_streamRunning)
    {
        // if it's not actively streaming and everything has been disarmed,
        // then stop buffering.
        ResetFifo();
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::GetArmed(_In_ GUID eventId, _Out_ BOOL *Arm)
{
    PAGED_CODE();
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (eventId == CONTOSO_KEYWORD1)
    {
        *Arm = m_SoundDetectorArmed1;
    }
    else if(eventId == CONTOSO_KEYWORD2)
    {
        *Arm = m_SoundDetectorArmed2;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::GetStreamingSupport(_In_ GUID eventId, _Out_ BOOL *Support)
{
    PAGED_CODE();
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (eventId == GUID_NULL)
    {
        *Support = TRUE;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID CKeywordDetector::Run()
{
    PAGED_CODE();

    if (m_qpcStartCapture == 0)
    {
        StartBufferingStream();
    }

    m_streamRunning = TRUE;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID CKeywordDetector::Stop()
{
    PAGED_CODE();

    ResetFifo();
    m_streamRunning = FALSE;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID CKeywordDetector::StartBufferingStream()
{
    LARGE_INTEGER qpc;
    LARGE_INTEGER qpcFrequency;

    PAGED_CODE();

    NT_ASSERT(m_qpcStartCapture == 0);
    NT_ASSERT(IsListEmpty(&PacketFifoHead));

    qpc = KeQueryPerformanceCounter(&qpcFrequency);
    m_qpcStartCapture = qpc.QuadPart;
    m_qpcFrequency = qpcFrequency.QuadPart;

    return;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID CKeywordDetector::NotifyDetection()
{
    PAGED_CODE();

    // A detection will only happen if armed and the
    // stream is already running. If there isn't a client
    // running, then set the stream start time to align
    // with this detection.
    if (!m_streamRunning)
    {
        StartBufferingStream();

        // The following code is for testing purposes only.
        // m_qpcFrequency is defined to be the number of ticks in 1 second.
        // Use the stream start time (the current time retrieved in StartBufferStream) to 
        // mark when the keyword ended, and the start time minus 1 second worth of ticks
        // to mark when the keyword started. Also, adjust the stream start time to align
        // to this new keyword start time, so that the simulated stream contains the full keyword.

        m_ullKeywordStopTimestamp = m_qpcStartCapture; // stop time is the current time
        m_qpcStartCapture = m_qpcStartCapture - m_qpcFrequency; // buffer start time is 1 second ago
        m_ullKeywordStartTimestamp = m_qpcStartCapture; // buffer start time = keyword start time

    }
    else
    {
        // The following code is for testing purposes only.
        // If the stream is running, we cannot modify qpcStartCapture to be in
        // the past, so instead make the keyword start & stop times fit within the
        // time period that the keyword has been running. If it has been running
        // for more than 1 second, then set the keyword start time to be 1 second back
        // into the stream, as though we just figured out there was a keyword there.
        // If it has been running less than one second, then the keyword size ends
        // up being however long the stream has been running. 

        LARGE_INTEGER qpc;
        qpc = KeQueryPerformanceCounter(NULL);

        m_ullKeywordStopTimestamp = qpc.QuadPart; // stop time is the current time

        if (m_qpcStartCapture < (qpc.QuadPart - m_qpcFrequency))
        {
            m_ullKeywordStartTimestamp = (qpc.QuadPart - m_qpcFrequency); 
        }
        else
        {
            m_ullKeywordStartTimestamp = m_qpcStartCapture;
        }

    }

    return;
}

#pragma code_seg()
_IRQL_requires_min_(DISPATCH_LEVEL)
VOID CKeywordDetector::DpcRoutine(_In_ LONGLONG PerformanceCounter, _In_ LONGLONG PerformanceFrequency)
{
    LONGLONG currentPacket;
    LONGLONG packetsToQueue;

    if (m_qpcStartCapture <= 0)
    {
        return;
    }

    currentPacket = (PerformanceCounter - m_qpcStartCapture) * (SamplesPerSecond / SamplesPerPacket) / PerformanceFrequency;
    packetsToQueue = currentPacket - m_nLastQueuedPacket;

    while (packetsToQueue > 0)
    {
        LIST_ENTRY*     packetListEntry;
        PACKET_ENTRY*   packetEntry;

        do
        {
            packetListEntry = ExInterlockedRemoveHeadList(&PacketPoolHead, &PacketPoolSpinLock);
            if (packetListEntry != NULL) break;

            // Pool is empty, no room to buffer more, an overrun is occurring. Drop and reuse the
            // oldest packet from head of fifo.

            // Since the pool is empty, the fifo should be full. However, although unlikely, the
            // driver might empty the fifo before this routine removes a packet. In that case, the
            // pool should have packets available again. Therefore this is a retry loop.
            packetListEntry = ExInterlockedRemoveHeadList(&PacketFifoHead, &PacketFifoSpinLock);
            if (packetListEntry != NULL) break;
        } while (TRUE);

        packetEntry = CONTAINING_RECORD(packetListEntry, PACKET_ENTRY, ListEntry);

        packetEntry->PacketNumber = ++m_nLastQueuedPacket;
        packetEntry->QpcWhenSampled = m_qpcStartCapture + (packetEntry->PacketNumber * PerformanceFrequency * SamplesPerPacket / SamplesPerSecond);

        RtlZeroMemory(&packetEntry->Samples[0], sizeof(packetEntry->Samples));

        InitializeListHead(packetListEntry);
        ExInterlockedInsertTailList(&PacketFifoHead, packetListEntry, &PacketFifoSpinLock);

        packetsToQueue -= 1;
    }
}

#pragma code_seg()
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CKeywordDetector::GetReadPacket
(
    _In_ ULONG PacketsPerWaveRtBuffer,
    _In_ ULONG WaveRtBufferSize,
    _Out_writes_(WaveRtBufferSize) BYTE *WaveRtBuffer,
    _Out_ ULONG *PacketNumber,
    _Out_ ULONG64 *PerformanceCounterValue,
    _Out_ BOOL *MoreData
)
{
    NTSTATUS ntStatus;
    BYTE *packetData;
    PACKET_ENTRY *packetEntry;
    LIST_ENTRY *packetListEntry = NULL;
    ULONG packetSize = WaveRtBufferSize / PacketsPerWaveRtBuffer;

    NT_ASSERT(SamplesPerPacket * 2 == packetSize);
    NT_ASSERT(sizeof(packetEntry->Samples) == packetSize);

    packetListEntry = ExInterlockedRemoveHeadList(&PacketFifoHead, &PacketFifoSpinLock);
    if (packetListEntry == NULL)
    {
        ntStatus = STATUS_DEVICE_NOT_READY;
        goto Exit;
    }
    packetEntry = CONTAINING_RECORD(packetListEntry, PACKET_ENTRY, ListEntry);

    packetData = WaveRtBuffer + ((packetEntry->PacketNumber * packetSize) % WaveRtBufferSize);

    ntStatus = RtlLongLongToULong(packetEntry->PacketNumber, PacketNumber);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *PerformanceCounterValue = packetEntry->QpcWhenSampled;
    *MoreData = !IsListEmpty(&PacketFifoHead);
    RtlCopyMemory(packetData, packetEntry->Samples, sizeof(packetEntry->Samples));

Exit:
    if (packetListEntry != NULL)
    {
        ExInterlockedInsertTailList(&PacketPoolHead, packetListEntry, &PacketPoolSpinLock);
    }

    return ntStatus;
}

#pragma code_seg()
