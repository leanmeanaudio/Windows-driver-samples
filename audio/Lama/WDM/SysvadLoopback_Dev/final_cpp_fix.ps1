# final_cpp_fix.ps1
# Comprehensive build script to fix Windows Audio Driver C++ issues

Write-Host "Applying C++ compatibility fixes for Windows Audio Driver..." -ForegroundColor Green

$projectDir = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev"
$projectFile = "$projectDir\SysvadLoopback_Dev.vcxproj"
$buildLogFile = "$projectDir\build_log.txt"

# Step 1: Create complete C++ wrapper header to address all compatibility issues
$cppWrapperContent = @"
#pragma once

// Master wrapper to fix C++ compatibility issues with Windows audio structures
// This header should be included before any system headers

// Disable warnings about bit-fields and anonymous structures
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4214) // bit field types other than int

// Force C mode for specific headers to avoid C++ bit-field issues
#ifdef __cplusplus
extern "C" {
#endif

// Define Windows types that might be missing
#ifndef WINAPI
#define WINAPI __stdcall
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// Define standard WAVEFORMATEX structure
#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct tWAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX;
#endif

// Define standard WAVEFORMATEXTENSIBLE structure
#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct tWAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

// Define standard KSDATAFORMAT_WAVEFORMATEXTENSIBLE structure
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct tKSDATAFORMAT_WAVEFORMATEXTENSIBLE {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif

// Common audio constants
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO 0x00000004
#endif

#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO 0x00000003
#endif

#ifdef __cplusplus
}
#endif

// Safe helper functions for KSDATARANGE structure access
inline void GetSafeChannelInfo(PKSDATARANGE DataRange, ULONG* pMinChannels, ULONG* pMaxChannels) {
    // Default values
    *pMinChannels = 1;        // Default minimum
    *pMaxChannels = 2;        // Default maximum (stereo)
    
    // Pointer-based access to avoid structure layout dependencies
    if (DataRange != NULL && 
        DataRange->FormatSize >= sizeof(KSDATAFORMAT) + sizeof(ULONG)) {
        
        // First ULONG after KSDATAFORMAT is MaximumChannels
        PULONG pChannels = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT));
        if (*pChannels > 0) {
            *pMaxChannels = *pChannels;
        }
    }
}

// Safe helper function to get bit depth information from a KSDATARANGE structure
inline void GetSafeBitDepthInfo(PKSDATARANGE DataRange, ULONG* pMinBitsPerSample, ULONG* pMaxBitsPerSample) {
    // Default values
    *pMinBitsPerSample = 16;  // Default minimum
    *pMaxBitsPerSample = 32;  // Default maximum
    
    // Pointer-based access to avoid structure layout dependencies
    if (DataRange != NULL && 
        DataRange->FormatSize >= sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG))) {
        
        // Second ULONG after KSDATAFORMAT is MinimumBitsPerSample
        // Third ULONG after KSDATAFORMAT is MaximumBitsPerSample
        PULONG pMinBits = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + sizeof(ULONG));
        PULONG pMaxBits = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + (2 * sizeof(ULONG)));
        
        if (*pMinBits > 0 && *pMaxBits > 0) {
            *pMinBitsPerSample = *pMinBits;
            *pMaxBitsPerSample = *pMaxBits;
        }
    }
}

// Safe helper function to get sample rate information from a KSDATARANGE structure
inline void GetSafeSampleRateInfo(PKSDATARANGE DataRange, ULONG* pMinSampleRate, ULONG* pMaxSampleRate) {
    // Default values
    *pMinSampleRate = 8000;    // Default minimum
    *pMaxSampleRate = 192000;  // Default maximum
    
    // Pointer-based access to avoid structure layout dependencies
    if (DataRange != NULL && 
        DataRange->FormatSize >= sizeof(KSDATAFORMAT) + (5 * sizeof(ULONG))) {
        
        // Fourth ULONG after KSDATAFORMAT is MinimumSampleFrequency
        // Fifth ULONG after KSDATAFORMAT is MaximumSampleFrequency
        PULONG pMinRate = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG)));
        PULONG pMaxRate = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + (4 * sizeof(ULONG)));
        
        if (*pMinRate > 0 && *pMaxRate > 0) {
            *pMinSampleRate = *pMinRate;
            *pMaxSampleRate = *pMaxRate;
        }
    }
}
"@

$cppWrapperFile = "$projectDir\cpp_compatibility.h"
Write-Host "Creating C++ compatibility header: $cppWrapperFile" -ForegroundColor Cyan
Set-Content -Path $cppWrapperFile -Value $cppWrapperContent

# Step 2: Create a response file for the compiler with all necessary options
$rspFileContent = @"
/D "_WIN32" 
/D "UNICODE" 
/D "_UNICODE" 
/D "PC_IMPLEMENTATION" 
/D "DEBUG_LEVEL=DEBUGLVL_TERSE" 
/D "_USE_WAVERT_" 
/D "SYSVAD_BTH_BYPASS" 
/D "SYSVAD_USB_SIDEBAND" 
/D "_USE_IPortClsRuntimePower" 
/D "_NEW_DELETE_OPERATORS_" 
/D "_ALLOW_KEYWORD_MACROS" 
/D "EXTERN_C=extern" 
/wd4201 
/wd4214 
/wd4115 
/wd4603 
/wd4627 
/wd4986 
/wd4987 
/wd4996 
/FI"cpp_compatibility.h"
"@

$rspFile = "$projectDir\build.rsp"
Write-Host "Creating compiler response file: $rspFile" -ForegroundColor Cyan
Set-Content -Path $rspFile -Value $rspFileContent

# Step 3: Create a header wrapper for specific files
$headerWrapperContent = @"
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
"@

$headerWrapperFile = "$projectDir\header_wrapper.h"
Write-Host "Creating header wrapper: $headerWrapperFile" -ForegroundColor Cyan
Set-Content -Path $headerWrapperFile -Value $headerWrapperContent

# Step 4: Run MSBuild with our response file
Write-Host "Running MSBuild with compatibility options..." -ForegroundColor Green
& "C:\Windows\Microsoft.NET\Framework64\v4.0.30319\MSBuild.exe" $projectFile /p:Configuration=Debug /p:Platform=x64 "/p:ForceImportBeforeCppTargets=$projectDir\build.rsp" "/p:AdditionalOptions=@$projectDir\build.rsp" "/p:AdditionalIncludeDirectories=$projectDir" /flp:"LogFile=$buildLogFile;Verbosity=detailed"

# Print the build log
if (Test-Path $buildLogFile) {
    Write-Host "Build log:" -ForegroundColor Yellow
    Get-Content $buildLogFile | ForEach-Object {
        if ($_ -match "error") {
            Write-Host $_ -ForegroundColor Red
        }
        elseif ($_ -match "warning") {
            Write-Host $_ -ForegroundColor Yellow
        }
        else {
            Write-Host $_
        }
    }
}

Write-Host "Build script completed. Check the build log for details." -ForegroundColor Green
