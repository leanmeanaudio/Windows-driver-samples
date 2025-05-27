#pragma once

// KS/KSMEDIA compatibility header for Windows audio driver
// Include this before <ks.h> and <ksmedia.h>

#ifndef KSMEDIA_COMPAT_H
#define KSMEDIA_COMPAT_H

// Fix for C++ compilation issues with KS.H and KSMEDIA.H
#ifdef __cplusplus

// Disable common warnings for driver development
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4214) // bit field types other than int
#pragma warning(disable: 4100) // unreferenced formal parameter

// Fix anonymous union issues in KSMEDIA structures
// These declarations eliminate the "unknown override specifier" errors (C3646)
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

#endif // __cplusplus

#endif // KSMEDIA_COMPAT_H
