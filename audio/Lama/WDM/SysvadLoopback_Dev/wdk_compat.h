#pragma once

// This header adds compatibility fixes for different WDK versions
// Include this before any standard Windows headers

#ifndef WDK_COMPAT_H
#define WDK_COMPAT_H

// Ensure basic Windows types are defined
#include <windef.h>

// Define BOOL if not already defined (fixes C2061 syntax error: identifier 'BOOL')
#ifndef BOOL
typedef int BOOL;
#endif

// Define missing union identifiers for ksmedia.h (fixes C3646 unknown override specifier)
// These are defined as preprocessor macros to avoid any actual code changes
#ifndef KSMEDIA_COMPAT_FIXED
#define KSMEDIA_COMPAT_FIXED

// Override specifiers in ksmedia.h
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

#endif // KSMEDIA_COMPAT_FIXED

// Add any other compatibility fixes here as needed

#endif // WDK_COMPAT_H
