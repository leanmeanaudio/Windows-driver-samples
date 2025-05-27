#pragma once

// This header will be forcibly included in all source files via /FI compiler option
// It ensures our compatibility fixes are applied before any system headers

// Disable C++ specific warnings that interfere with driver compilation
#ifdef __cplusplus
// Prevent structure packing/alignment problems in C++ mode
#pragma pack(push, 8)

// Disable warnings that are problematic for Windows driver code
#pragma warning(disable: 4201)  // nameless struct/union
#pragma warning(disable: 4214)  // bit field types other than int
#pragma warning(disable: 4505)  // unreferenced local function has been removed
#pragma warning(disable: 4768)  // missing braces around initializer

// Force C linkage for problematic functions and structures
#define EXTERN_GUID extern "C" const GUID
#define DEFINE_GUID extern "C" const GUID
#define EXTERN_C extern "C"

// Allow use of C language keywords that are also C++ keywords
#define _ALLOW_KEYWORD_MACROS 1

// Define alternate access macros for array syntax
#define ARRAY_ACCESS(arr, idx) arr.Data[idx]
#endif

// Include our compatibility headers first
#include "cpp_compatibility.h"
#include "ksmedia_override.h"

// Fix for missing MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// Ensure BOOL type is defined
#ifndef BOOL
typedef int BOOL;
#endif

// Override specific problematic structures in ksmedia.h
#ifdef __cplusplus
// Disable some specific ksmedia.h errors by overriding problematic macros
#define KS_DVD_CSS_CHALLENGE KS_DVD_CSS_CHALLENGE_SAFE
#define KS_DVD_CSS_KEY KS_DVD_CSS_KEY_SAFE
#define KS_DVD_DISC_KEY KS_DVD_DISC_KEY_SAFE

// Handle KSPROPERTY_COMPOSIT_ON properly
#define KSPROPERTY_COMPOSIT_ON 0
#define KSPROPERTY_COMPOSIT_OFF 1
#endif

// Add workarounds for bit-field issues
#define REDEFINE_BITFIELDS 1

// Redefine bit-field structures in system headers to avoid the "unknown override specifier" errors
#ifdef __cplusplus
// Define helper macros to replace bit-field definitions with regular members
#define __KSBITFIELD_FIELD(x,y) ULONG x
#define __KSBITFIELD_DUMMY ULONG dummy

// Override problematic bit-field definitions in system headers
#define BIT_FIELD_OVERRIDE 1

#pragma push_macro("Looped")
#pragma push_macro("InROM")
#pragma push_macro("fRepeatPreviousBlock")
#pragma push_macro("fErrorInCurrentBlock")
#pragma push_macro("fStereo")
#pragma push_macro("fDownMix") 
#pragma push_macro("fDolbySurround")
#pragma push_macro("fLargeRoom")
#pragma push_macro("InterleavedChannelStartPosition")
#pragma push_macro("InterleavedChannelCount")
#pragma push_macro("InterleavedChannelMask")
#pragma push_macro("PrimaryChannelCount")
#pragma push_macro("PrimaryChannelStartPosition")
#pragma push_macro("PrimaryChannelMask")

#undef Looped
#undef InROM
#undef fRepeatPreviousBlock
#undef fErrorInCurrentBlock
#undef fStereo
#undef fDownMix
#undef fDolbySurround
#undef fLargeRoom
#undef InterleavedChannelStartPosition
#undef InterleavedChannelCount
#undef InterleavedChannelMask
#undef PrimaryChannelCount
#undef PrimaryChannelStartPosition
#undef PrimaryChannelMask

// Create safe replacements
#define Looped __KSBITFIELD_FIELD(Looped, 1)
#define InROM __KSBITFIELD_FIELD(InROM, 1)
#define fRepeatPreviousBlock __KSBITFIELD_FIELD(fRepeatPreviousBlock, 1)
#define fErrorInCurrentBlock __KSBITFIELD_FIELD(fErrorInCurrentBlock, 1)
#define fStereo __KSBITFIELD_FIELD(fStereo, 1)
#define fDownMix __KSBITFIELD_FIELD(fDownMix, 1)
#define fDolbySurround __KSBITFIELD_FIELD(fDolbySurround, 1)
#define fLargeRoom __KSBITFIELD_FIELD(fLargeRoom, 1)
#define InterleavedChannelStartPosition __KSBITFIELD_FIELD(InterleavedChannelStartPosition, 8)
#define InterleavedChannelCount __KSBITFIELD_FIELD(InterleavedChannelCount, 8)
#define InterleavedChannelMask __KSBITFIELD_FIELD(InterleavedChannelMask, 32)
#define PrimaryChannelCount __KSBITFIELD_FIELD(PrimaryChannelCount, 8)
#define PrimaryChannelStartPosition __KSBITFIELD_FIELD(PrimaryChannelStartPosition, 8)
#define PrimaryChannelMask __KSBITFIELD_FIELD(PrimaryChannelMask, 32)

#endif // __cplusplus
