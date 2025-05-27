#pragma once

// Master compatibility header for Windows audio driver compilation
// This should be included before any other Windows headers

#ifndef DRIVER_COMPAT_H
#define DRIVER_COMPAT_H

// Don't redefine these if they're already defined
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7 compatibility
#endif

#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif

// Compatibility with different WDK versions
#ifdef __cplusplus
// Disable warnings about anonymous structs
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4214) // bit field types other than int
#pragma warning(disable: 4115) // named type definition in parentheses
#pragma warning(disable: 4408) // anonymous struct did not declare any data members

// No need to define macros here - moved to fix_ks_cpp.h
#endif // __cplusplus

// Include our specialized compatibility headers
#include "ksdatarange_compat.h"
#include "ksmedia_compat.h"

// Basic Windows type definitions
#ifndef BOOL
typedef int BOOL;
#endif

// Fix for KS.H compatibility issues
#ifndef _KS_
#define _KS_

// Fix for function declarations in ks.h
#ifndef KSDDKAPI
#define KSDDKAPI
#endif

// Fix override specifiers in ksmedia.h
#define x x
#define y y
#define z z
#define dvX dvX
#define dvY dvY
#define dvZ dvZ
#define DistanceFactor DistanceFactor
#define RolloffFactor RolloffFactor
#define DopplerFactor DopplerFactor
#define MinDistance MinDistance

// Ensure standard compiler settings
#pragma warning(disable: 4603)  // Warning about old-style C++
#pragma warning(disable: 4627)  // Header inclusion order
#pragma warning(disable: 4986)  // Exception specification behavior
#pragma warning(disable: 4987)  // nonstandard extension used
#pragma warning(disable: 4996)  // Deprecated functions

// Allow multiple struct definitions with same name across headers
#pragma warning(disable: 4201)  // nonstandard extension used: nameless struct/union

// Additional helper definitions
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }
#endif

#endif // _KS_

#endif // DRIVER_COMPAT_H
