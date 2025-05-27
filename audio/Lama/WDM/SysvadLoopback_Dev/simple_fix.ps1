# simple_fix.ps1
# Focused script to fix specific compilation issues in SysvadLoopback_Dev driver

Write-Host "Applying targeted fixes to SysvadLoopback_Dev driver..." -ForegroundColor Cyan

# Set environment variables for MSBuild with specific compiler flags
$env:CL = "/D_WINDLL /DDRIVER_FIXED"

# Create a file with a forward declaration of KSDATAFORMAT_WAVEFORMATEXTENSIBLE
$ksdataFormatHeader = @"
// Forward declaration of KSDATAFORMAT_WAVEFORMATEXTENSIBLE
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif
"@

$forwardDeclFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\ksdataformat_forward.h"
Set-Content -Path $forwardDeclFile -Value $ksdataFormatHeader
Write-Host "Created forward declaration file for KSDATAFORMAT_WAVEFORMATEXTENSIBLE" -ForegroundColor Green

# Modify hdmitopo.cpp to include our forward declaration
$hdmiTopoFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\hdmitopo.cpp"
if (Test-Path $hdmiTopoFile) {
    $content = Get-Content -Path $hdmiTopoFile -Raw
    $pattern = "#include <ksmedia.h>"
    $replacement = "#include <ksmedia.h>`n#include `"ksdataformat_forward.h`""
    $newContent = $content -replace $pattern, $replacement
    Set-Content -Path $hdmiTopoFile -Value $newContent
    Write-Host "Updated hdmitopo.cpp to include forward declaration" -ForegroundColor Green
}

# Do the same for micintopo.cpp
$micinTopoFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\micintopo.cpp"
if (Test-Path $micinTopoFile) {
    $content = Get-Content -Path $micinTopoFile -Raw
    $pattern = "#include <portcls.h>"
    $replacement = "#include <portcls.h>`n#include `"ksdataformat_forward.h`""
    $newContent = $content -replace $pattern, $replacement
    Set-Content -Path $micinTopoFile -Value $newContent
    Write-Host "Updated micintopo.cpp to include forward declaration" -ForegroundColor Green
}

# Now build the project with minimal options
Write-Host "Building the driver with targeted fixes..." -ForegroundColor Yellow
$buildResult = msbuild /p:Platform=x64 /p:Configuration=Debug /v:minimal 2>&1

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host "Build output:" -ForegroundColor Yellow
    $buildResult | ForEach-Object { Write-Host $_ -ForegroundColor Gray }
}
