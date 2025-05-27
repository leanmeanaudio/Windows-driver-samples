@echo off
echo Building SysvadLoopback_Dev driver with comprehensive audio driver compatibility fixes...

:: Set compiler options for compatibility
set CL=/DDRIVER_COMPATIBILITY_MODE

:: Run MSBuild with appropriate options
msbuild /p:Platform=x64 /p:Configuration=Debug /v:minimal

:: Check the build result
if %ERRORLEVEL% == 0 (
  echo Build completed successfully!
) else (
  echo Build failed with error level %ERRORLEVEL%
  echo Please check the compilation errors above.
)
