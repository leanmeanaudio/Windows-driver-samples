@echo off
echo Building SysvadLoopback_Dev driver with basic compatibility options...

:: Set basic compiler options
set CL=/DWINDOWS_DRIVER /D_WINDLL /DDRIVER_FIXED

:: Run MSBuild with simple options
msbuild /p:Platform=x64 /p:Configuration=Debug /v:minimal

:: Check the build result
if %ERRORLEVEL% == 0 (
  echo Build completed successfully!
) else (
  echo Build failed with error level %ERRORLEVEL%
)
