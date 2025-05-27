# final_portcls_fix.ps1
# Direct approach to fix portcls.h compatibility issues

Write-Host "Applying targeted portcls.h compatibility fix..." -ForegroundColor Cyan

# Define the main source files to update
$sourceFiles = @(
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\micintopo.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\hdmitopo.cpp"
)

# Add our portcls_compat.h header to each file BEFORE any other includes
foreach ($file in $sourceFiles) {
    if (Test-Path $file) {
        $content = Get-Content -Path $file -Raw
        
        # Prepend our compatibility header
        $newContent = "// Compatibility fix for portcls.h must be first include`n#include `"portcls_compat.h`"`n`n$content"
        Set-Content -Path $file -Value $newContent
        Write-Host "Updated $file to include portcls_compat.h first" -ForegroundColor Green
    }
}

# Create a simple build batch script that uses minimal compiler options
$buildBatch = @"
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
"@

$buildBatchFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\build_with_portcls_fix.bat"
Set-Content -Path $buildBatchFile -Value $buildBatch
Write-Host "Created build batch file with portcls.h compatibility options" -ForegroundColor Green

# Run the build batch file
Write-Host "Running the build batch file..." -ForegroundColor Yellow
& cmd.exe /c $buildBatchFile

Write-Host "Build process completed." -ForegroundColor Cyan
