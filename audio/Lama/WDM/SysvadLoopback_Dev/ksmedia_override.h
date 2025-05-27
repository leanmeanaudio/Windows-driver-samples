#pragma once

// This file provides complete replacements for problematic structures in ksmedia.h
// to avoid C++ syntax errors with array declarations and bit-fields

// Include this file BEFORE ksmedia.h is included

// Fix for KSPROPERTY_COMPOSIT_ON issue
#ifndef KSPROPERTY_COMPOSIT_ON_DEFINED
#define KSPROPERTY_COMPOSIT_ON_DEFINED
typedef enum {
    KSPROPERTY_COMPOSIT_ON = 0,
    KSPROPERTY_COMPOSIT_OFF
} KSPROPERTY_COMPOSIT_ENUM;
#endif

// Override problematic DVD CSS structures with safe versions
#ifndef DVD_CHALLENGE_KEY_STRUCT_DEFINED
#define DVD_CHALLENGE_KEY_STRUCT_DEFINED

#ifdef __cplusplus
// In C++ mode, completely override the problematic structures

// First define wrapper structures for the arrays
typedef struct _DVD_CHALLENGE_KEY {
    ULONG Data[2];
    
    // Add operator[] for compatibility with array access syntax
    ULONG& operator[](int index) { return Data[index]; }
    const ULONG& operator[](int index) const { return Data[index]; }
} DVD_CHALLENGE_KEY, *PDVD_CHALLENGE_KEY;

typedef struct _DVD_BUS_KEY {
    ULONG Data[2];
    
    // Add operator[] for compatibility with array access syntax
    ULONG& operator[](int index) { return Data[index]; }
    const ULONG& operator[](int index) const { return Data[index]; }
} DVD_BUS_KEY, *PDVD_BUS_KEY;

typedef struct _DVD_DISC_KEY {
    ULONG Data[2];
    
    // Add operator[] for compatibility with array access syntax
    ULONG& operator[](int index) { return Data[index]; }
    const ULONG& operator[](int index) const { return Data[index]; }
} DVD_DISC_KEY, *PDVD_DISC_KEY;

// KS_DVD structures with our custom types
typedef struct _KS_DVD_CSS_CHALLENGE_SAFE {
    DVD_CHALLENGE_KEY ChlgKey;
    ULONG Reserved[2];
} KS_DVD_CSS_CHALLENGE_SAFE, *PKS_DVD_CSS_CHALLENGE_SAFE;

typedef struct _KS_DVD_CSS_KEY_SAFE {
    DVD_BUS_KEY BusKey;
    ULONG Reserved[2];
} KS_DVD_CSS_KEY_SAFE, *PKS_DVD_CSS_KEY_SAFE;

typedef struct _KS_DVD_DISC_KEY_SAFE {
    DVD_DISC_KEY DiscKey;
} KS_DVD_DISC_KEY_SAFE, *PKS_DVD_DISC_KEY_SAFE;

// Bit mask structure with operator[]
typedef struct _KS_BITMASK_STRUCT {
    ULONG Data[3];
    
    // Add operator[] for compatibility with array access syntax
    ULONG& operator[](int index) { return Data[index]; }
    const ULONG& operator[](int index) const { return Data[index]; }
} KS_BITMASK_STRUCT, *PKS_BITMASK_STRUCT;

// Define the macros to use our safe versions
#define KS_DVD_CSS_CHALLENGE_OVERRIDE KS_DVD_CSS_CHALLENGE_SAFE
#define KS_DVD_CSS_KEY_OVERRIDE KS_DVD_CSS_KEY_SAFE
#define KS_DVD_DISC_KEY_OVERRIDE KS_DVD_DISC_KEY_SAFE

#else
// In C mode, use the original structures
typedef struct _DVD_CHALLENGE_KEY {
    ULONG Data[2];
} DVD_CHALLENGE_KEY, *PDVD_CHALLENGE_KEY;

typedef struct _DVD_BUS_KEY {
    ULONG Data[2];
} DVD_BUS_KEY, *PDVD_BUS_KEY;

typedef struct _DVD_DISC_KEY {
    ULONG Data[2];
} DVD_DISC_KEY, *PDVD_DISC_KEY;
#endif

#endif // DVD_CHALLENGE_KEY_STRUCT_DEFINED
