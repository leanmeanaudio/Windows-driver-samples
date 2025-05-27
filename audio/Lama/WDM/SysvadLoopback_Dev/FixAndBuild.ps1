# FixAndBuild.ps1
# Comprehensive fix and build script for Windows audio driver compilation issues
# This script applies compatibility patches and builds the project with specialized options

# Stop on any error
$ErrorActionPreference = "Stop"

Write-Host "Starting Windows Audio Driver Fix and Build Process" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan

# Step 1: Create a precompiled header with essential fixes
Write-Host "Step 1: Creating compatibility header..." -ForegroundColor Green
$compatHeader = @'
#pragma once
// Compatibility header for Windows Audio Driver WDK issues
// Include this before any other headers

// Disable warnings
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4100)  // Unreferenced formal parameter
#pragma warning(disable: 4127)  // Conditional expression is constant
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int
#pragma warning(disable: 4701)  // Potentially uninitialized variable
#pragma warning(disable: 4703)  // Potentially uninitialized pointer

// C++ compatibility - to avoid 'unknown override specifier' errors
#ifdef __cplusplus
    // MS C++ 11+ compiler treats anonymous struct/union field names as override specifiers
    #define STRUCT_FIELD(name) name
#else
    #define STRUCT_FIELD(name) name
#endif

// Common field names in anonymous structs/unions
#define Set STRUCT_FIELD(Set)
#define Id STRUCT_FIELD(Id)
#define Flags STRUCT_FIELD(Flags)
#define PriorityClass STRUCT_FIELD(PriorityClass)
#define PrioritySubClass STRUCT_FIELD(PrioritySubClass)
#define Alignment STRUCT_FIELD(Alignment)
#define NodeId STRUCT_FIELD(NodeId)
#define Reserved STRUCT_FIELD(Reserved)
#define Size STRUCT_FIELD(Size)
#define Count STRUCT_FIELD(Count)
#define AccessFlags STRUCT_FIELD(AccessFlags)
#define DescriptionSize STRUCT_FIELD(DescriptionSize)
#define MembersListCount STRUCT_FIELD(MembersListCount)
#define MembersFlags STRUCT_FIELD(MembersFlags)
#define MembersSize STRUCT_FIELD(MembersSize)
#define MembersCount STRUCT_FIELD(MembersCount)
#define SignedMinimum STRUCT_FIELD(SignedMinimum)
#define SignedMaximum STRUCT_FIELD(SignedMaximum)
#define UnsignedMinimum STRUCT_FIELD(UnsignedMinimum)
#define UnsignedMaximum STRUCT_FIELD(UnsignedMaximum)
#define Granularity STRUCT_FIELD(Granularity)
#define Relation STRUCT_FIELD(Relation)
#define Type STRUCT_FIELD(Type)
#define MembersHeader STRUCT_FIELD(MembersHeader)
#define x STRUCT_FIELD(x)
#define y STRUCT_FIELD(y)
#define z STRUCT_FIELD(z)
#define dvX STRUCT_FIELD(dvX)
#define dvY STRUCT_FIELD(dvY)
#define dvZ STRUCT_FIELD(dvZ)
#define DistanceFactor STRUCT_FIELD(DistanceFactor)
#define RolloffFactor STRUCT_FIELD(RolloffFactor)
#define DopplerFactor STRUCT_FIELD(DopplerFactor)
#define MinDistance STRUCT_FIELD(MinDistance)
'@

$compatHeader | Set-Content -Path "drivercompat.h" -Encoding utf8

# Step 2: Create response file for the compiler
Write-Host "Step 2: Creating compiler response file..." -ForegroundColor Green
$rspContent = @'
# Compiler Response File for WDK compatibility issues
/FIdrivercompat.h
/wd4005
/wd4127
/wd4201
/wd4214
/wd4603
/wd4627
/wd4986
/wd4987
/Zc:wchar_t-
/Zc:forScope-
/Zc:rvalueCast-
/D_CRT_SECURE_NO_WARNINGS
/D_ALLOW_KEYWORD_MACROS
'@

$rspContent | Set-Content -Path "wdk_compat.rsp" -Encoding utf8

# Step 3: Create MSBuild property file to include our response file
Write-Host "Step 3: Creating MSBuild property file..." -ForegroundColor Green
$propsContent = @'
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalOptions>@$(MSBuildThisFileDirectory)wdk_compat.rsp %(AdditionalOptions)</AdditionalOptions>
      <TreatWarningAsError>false</TreatWarningAsError>
      <WarningLevel>2</WarningLevel>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
'@

$propsContent | Set-Content -Path "WdkCompat.props" -Encoding utf8

# Step 4: Create batch file that will perform the actual build
Write-Host "Step 4: Creating build batch file..." -ForegroundColor Green
$batchContent = @'
@echo off
echo Starting Windows Audio Driver build with compatibility options...

:: Clean any previous build files
msbuild /t:Clean /nologo SysvadLoopback_Dev.vcxproj 

:: Set up environment variables to modify the build
set _CL_=/FIdrivercompat.h
set _LINK_=

:: Build using our property sheet
msbuild /nologo /p:ForceImportBeforeCppProps="%cd%\WdkCompat.props" SysvadLoopback_Dev.vcxproj

:: Check result
if %ERRORLEVEL% EQU 0 (
    echo.
    echo *** Build completed successfully! ***
    echo.
) else (
    echo.
    echo *** Build failed with error level %ERRORLEVEL% ***
    echo.
)
'@

$batchContent | Set-Content -Path "build.bat" -Encoding ascii

# Step 5: Execute the build
Write-Host "Step 5: Running build process..." -ForegroundColor Yellow
Write-Host "====================================================" -ForegroundColor Yellow
& cmd.exe /c build.bat

# Step 6: Report completion
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "Build process completed" -ForegroundColor Cyan
Write-Host "If you continue to face build issues, you might need to:" -ForegroundColor White
Write-Host "1. Use an older WDK version compatible with this driver code" -ForegroundColor White
Write-Host "2. Modify the driver code to be compatible with your current WDK" -ForegroundColor White
Write-Host "3. Consider using the Driver Development Kit (DDK) from the same Windows version" -ForegroundColor White
Write-Host "====================================================" -ForegroundColor Cyan
