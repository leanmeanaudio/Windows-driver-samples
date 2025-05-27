#pragma once

// This is a fix for the Windows audio driver headers
// It ensures all necessary types are defined in the correct order

// Include Windows multimedia headers first to get WAVEFORMATEX definition
#include <mmsystem.h>

// Include standard Windows kernel headers
#include <ntddk.h>
#include <wdm.h>

// Then include KS headers
#include <ks.h>
#include <ksmedia.h>

// Define KSDATAFORMAT_WAVEFORMATEXTENSIBLE if not already defined
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif

// Fix for anonymous struct/union field names that can be mistaken for C++ override specifiers
#ifdef __cplusplus
#define PriorityClass PriorityClass
#define PrioritySubClass PrioritySubClass
#define Set Set
#define Id Id
#define Flags Flags
#define Alignment Alignment
#define NodeId NodeId
#define Reserved Reserved
#define Size Size
#define Count Count
#define Type Type
#define Node Node
#define PinId PinId
#endif
