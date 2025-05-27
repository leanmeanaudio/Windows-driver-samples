// This file is automatically included at the start of each .cpp file
// It ensures our compatibility headers are included before system headers

#include "cpp_compatibility.h"

// Include other system headers here
#include <ntddk.h>
#include <wdm.h>
#include <windef.h>

// Ensure these standard audio headers come after our compatibility headers
#include <ks.h>
#include <ksmedia.h>
#include <portcls.h>
