# Build script for SysvadLoopback_Dev driver
# This script applies compatibility fixes and builds the project

# Stop on any error
$ErrorActionPreference = "Stop"

Write-Host "Setting up build environment for SysvadLoopback_Dev driver..." -ForegroundColor Green

# Create the compatibility pre-compiler header
$precompHeader = @'
// Auto-generated pre-compiler header to fix compatibility issues
#pragma once

// Prevent redefinition errors
#pragma warning(disable: 4005)  // Macro redefinition

// Disable specific warnings
#pragma warning(disable: 4100)  // Unreferenced formal parameter
#pragma warning(disable: 4127)  // Conditional expression is constant
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int
#pragma warning(disable: 4996)  // Deprecated functions

// Fix C++ handling of anonymous unions
#ifdef __cplusplus
// Anonymous struct/union field names
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
#define AccessFlags AccessFlags
#define DescriptionSize DescriptionSize
#define MembersListCount MembersListCount
#define MembersFlags MembersFlags
#define MembersSize MembersSize
#define MembersCount MembersCount
#define SignedMinimum SignedMinimum
#define SignedMaximum SignedMaximum
#define UnsignedMinimum UnsignedMinimum
#define UnsignedMaximum UnsignedMaximum
#define Granularity Granularity
#define Relation Relation
#define Type Type
#define MembersHeader MembersHeader
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
#endif
'@

# Save the pre-compiler header
$precompHeader | Out-File -FilePath "precomp.h" -Encoding utf8

# Create a batch file for building with modified options
$buildBatch = @'
@echo off
echo Building SysvadLoopback_Dev driver with compatibility options...

rem Clean any previous build
msbuild /t:Clean /nologo

rem Build with special options to bypass compatibility issues
msbuild /nologo /p:AdditionalOptions="/FIprecomp.h /wd4603 /wd4627 /wd4986 /wd4987 /Zc:wchar_t- /Zc:forScope- /Zc:rvalueCast-" /p:LanguageStandard=stdcpp14

rem Report build status
if %ERRORLEVEL% EQU 0 (
    echo Build completed successfully!
) else (
    echo Build failed with error level %ERRORLEVEL%.
)
'@

# Save the build batch file
$buildBatch | Out-File -FilePath "build_with_compat.bat" -Encoding ascii

# Execute the batch file
Write-Host "Starting build process..." -ForegroundColor Yellow
cmd /c build_with_compat.bat

# Check the result
if ($LASTEXITCODE -eq 0) {
    Write-Host "Driver build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Driver build failed with error code $LASTEXITCODE" -ForegroundColor Red
}
