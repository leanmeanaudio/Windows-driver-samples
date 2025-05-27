@echo off
echo Building SysvadLoopback_Dev driver with portcls.h compatibility fix...

:: Set minimal compiler options
set CL=/DPORTCLS_COMPAT_FIX

:: Run MSBuild
msbuild /p:Platform=x64 /p:Configuration=Debug /v:minimal

:: Check the build result
if %ERRORLEVEL% == 0 (
  echo Build completed successfully!
) else (
  echo Build failed with error level %ERRORLEVEL%
  echo Please check the compilation errors above.
)
