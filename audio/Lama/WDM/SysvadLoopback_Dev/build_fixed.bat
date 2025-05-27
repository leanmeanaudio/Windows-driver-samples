@echo off
echo Building SysvadLoopback_Dev driver with compatibility options...

:: Set compiler options to fix common issues
set CL=/FI"C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\driver_precompiled.h" /D_WINDLL /DDRIVER_FIXED

:: Run MSBuild with specific options
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" /p:Platform=x64 /p:Configuration=Debug /v:minimal

:: Check the build result
if %ERRORLEVEL% == 0 (
  echo Build completed successfully!
) else (
  echo Build failed with error level %ERRORLEVEL%
)
