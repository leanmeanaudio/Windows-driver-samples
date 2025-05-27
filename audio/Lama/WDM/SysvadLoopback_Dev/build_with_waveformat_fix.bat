@echo off
echo Building SysvadLoopback_Dev driver with WAVEFORMATEXTENSIBLE fix...

:: Set minimal compiler options
set CL=/DDRIVER_FIXED /DKSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED

:: Run MSBuild
msbuild /p:Platform=x64 /p:Configuration=Debug /v:normal

:: Check the build result
if %ERRORLEVEL% == 0 (
  echo Build completed successfully!
) else (
  echo Build failed with error level %ERRORLEVEL%
  echo Please check the compilation errors above.
)
