# final_build_fixes.ps1
# Complete solution for fixing build issues in SysvadLoopback_Dev driver

Write-Host "Applying comprehensive fixes to SysvadLoopback_Dev driver..." -ForegroundColor Cyan

# 1. Fix the hdmitopo.cpp file to include our compatibility headers
$hdmiTopoFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\hdmitopo.cpp"
if (Test-Path $hdmiTopoFile) {
    $content = Get-Content -Path $hdmiTopoFile -Raw
    if ($content -notmatch "sysvad_compat\.h") {
        $newContent = $content -replace "#include <ntddk.h>", "// Include our compatibility header first`n#include `"sysvad_compat.h`"`n`n#include <ntddk.h>"
        Set-Content -Path $hdmiTopoFile -Value $newContent
        Write-Host "Updated hdmitopo.cpp to include compatibility header" -ForegroundColor Green
    }
}

# 2. Create a link to the fixed DataRangeIntersection implementation
$sourceFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\fix_lamaloopbackrender.cpp"
$targetFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp"

if (Test-Path $sourceFile -and Test-Path $targetFile) {
    # Get the fixed implementation
    $fixedImplementation = Get-Content -Path $sourceFile -Raw
    $fixPattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection[\s\S]*?return STATUS_SUCCESS;\s*\}"
    $fixMatch = [regex]::Match($fixedImplementation, $fixPattern)
    
    if ($fixMatch.Success) {
        $originalImplementation = Get-Content -Path $targetFile -Raw
        $originalPattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection[\s\S]*?return STATUS_SUCCESS;\s*\}"
        
        # Replace the implementation in the target file
        $newImplementation = [regex]::Replace($originalImplementation, $originalPattern, $fixMatch.Value)
        Set-Content -Path $targetFile -Value $newImplementation
        
        Write-Host "Updated DataRangeIntersection implementation in lamaloopbackrender.cpp" -ForegroundColor Green
    } else {
        Write-Host "Could not find fixed DataRangeIntersection implementation in fix_lamaloopbackrender.cpp" -ForegroundColor Red
    }
} else {
    Write-Host "Source or target file not found" -ForegroundColor Red
}

# 3. Set build environment with compatibility options
$env:CL = "/DKSMEDIA_FIXED_INCLUDED /D_WINDLL /DDRIVER_FIXED /DUSING_PRECOMPILED_HEADERS /DNO_KS_ANONYMOUS_STRUCTURES /DNO_PREFETCH_HACK /FIc:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\sysvad_compat.h"
$env:INCLUDE = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev;C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\EndpointsCommon;$env:INCLUDE"

Write-Host "Set build environment variables:" -ForegroundColor Yellow
Write-Host "CL = $env:CL" -ForegroundColor Gray
Write-Host "INCLUDE path additions:" -ForegroundColor Gray
Write-Host "  C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev" -ForegroundColor Gray
Write-Host "  C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\EndpointsCommon" -ForegroundColor Gray

# 4. Build the project
Write-Host "Building the driver with compatibility options..." -ForegroundColor Yellow
msbuild /p:Platform=x64 /p:Configuration=Debug /p:IncludePath="C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev;C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\EndpointsCommon;$(IncludePath)" /v:normal

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host "Please check the error messages above for details." -ForegroundColor Yellow
}
