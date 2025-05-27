# build_with_cl_options.ps1
# Build script with compiler options to handle Windows Driver Kit compatibility issues

# Set environment variables for MSBuild with appropriate compiler flags
$env:CL = "/DKSMEDIA_FIXED /DWIN32 /D_AMD64_ /FIc:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\sysvad_compat.h"
$env:INCLUDE = "c:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev;$env:INCLUDE"

Write-Host "Building driver with custom CL options..." -ForegroundColor Cyan
Write-Host "CL options: $env:CL" -ForegroundColor Yellow

# Build the project
msbuild /p:Platform=x64 /p:Configuration=Debug /v:normal

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
}
