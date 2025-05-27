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
