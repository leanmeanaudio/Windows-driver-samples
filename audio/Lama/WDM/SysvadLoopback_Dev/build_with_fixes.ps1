# Build script with compatibility fixes for Lama SysvadLoopback_Dev driver
# This script should be run from the project directory

# Make sure we're in the right directory
$projectDir = Get-Location
Write-Host "Building project in: $projectDir" -ForegroundColor Cyan

# Set environment variables for MSBuild
$env:CL = "/D_CRT_SECURE_NO_WARNINGS /D_ATL_NO_DEFAULT_LIBS"

# Build the project using MSBuild with normal verbosity
Write-Host "Starting build..." -ForegroundColor Green
msbuild /p:Platform=x64 /p:Configuration=Debug /v:n

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
}
