#pragma once

// Precompiled header to fix Windows audio driver compilation issues
// This file addresses common problems with WDK headers and structure definitions

// Disable warnings that cause issues with WDK headers
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int

// Forward declaration of critical structures
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif

// Fix C++ anonymous struct/union field name issues
#ifdef __cplusplus
// Define field names that might be interpreted as C++11 override specifiers
#define Set Set
#define Id Id
#define Type Type
#define Node Node
#define PinId PinId
#define Reserved Reserved
#define Flags Flags
#endif
