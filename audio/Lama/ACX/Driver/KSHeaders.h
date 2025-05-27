#pragma once

// Ensure kernel mode is defined before any headers are included.
#ifndef _KERNEL_MODE_
#define _KERNEL_MODE_ 1
#endif

#ifndef KERNEL_MODE
#define KERNEL_MODE 1
#endif

//------------------------------------------------------------
// Core Kernel & WDF (most fundamental headers first)
//------------------------------------------------------------
#include <ntddk.h>       // Should be first for fundamental types like NTSTATUS, etc.
#include <windef.h>      // Explicitly include for WORD, DWORD, BOOL etc.
#include <wdf.h>         // WDF
#include <wdfpdo.h>      // For WdfDeviceInitSetPnpCapabilities
#include <wdfio.h>       // For additional WDF I/O functions
#include <wdmguid.h>     // For DEFINE_DEVPROPKEY etc.
#include <devpropdef.h>  // For DEVPROPID, DEVPROPTYPE
#include <ntstrsafe.h>   // For RtlStringCbPrintfW etc.

//------------------------------------------------------------
// Kernel Streaming & Multimedia headers
//------------------------------------------------------------
// Include our custom header for BITMAPINFOHEADER definition
// This MUST come before mmreg.h and ksmedia.h, but after core WDK headers
#include "bmi_fix.h"     // This includes mmreg.h safely
#include <ks.h>          // Kernel Streaming base
#include <ksmedia.h>     // KS media definitions

//------------------------------------------------------------
// Audio Class Extensions (ACX) headers
//------------------------------------------------------------
// Ensure ACX_VERSION defines are present before including acx.h.
// This is critical for proper versioning of the ACX interfaces
#define ACX_VERSION_MAJOR 1
#define ACX_VERSION_MINOR 1

// Include all ACX headers explicitly and in the correct order
// Ordering is important as some headers depend on definitions in others
#include <acx.h>                 // Main ACX include with core definitions
#include <acxcircuit.h>          // For ACXCIRCUIT_INIT and other circuit related functions
#include <acxpin.h>              // For ACX pin functions and constants like AcxPinTypeBridge
#include <acxdataformat.h>       // For ACXDATAFORMAT_CONFIG and related functions
#include <acxstreams.h>          // For ACX_RTPACKET and streaming functions
#include <acxtargets.h>          // For target pin operations

// Note: acxutil.h and acxsession.h are not available in this WDK version