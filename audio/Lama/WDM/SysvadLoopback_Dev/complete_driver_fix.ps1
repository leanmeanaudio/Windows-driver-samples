# complete_driver_fix.ps1
# Complete solution for fixing Windows audio driver compilation issues

Write-Host "Applying comprehensive fix for Windows audio driver..." -ForegroundColor Cyan

# Step 1: Identify key source files that need our compatibility header
$mainSourceFiles = @(
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\micintopo.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\hdmitopo.cpp"
)

# Step 2: Add our audio_compat.h header at the beginning of each source file
foreach ($file in $mainSourceFiles) {
    if (Test-Path $file) {
        $content = Get-Content -Path $file -Raw
        
        # Check if our header is already included
        if ($content -notmatch "audio_compat\.h") {
            # Prepend our compatibility header as the very first include
            $newContent = "// Include our audio driver compatibility header first`n#include `"audio_compat.h`"`n`n$content"
            Set-Content -Path $file -Value $newContent
            Write-Host "Updated $file to include audio_compat.h as first include" -ForegroundColor Green
        } else {
            Write-Host "File $file already includes audio_compat.h" -ForegroundColor Yellow
        }
    } else {
        Write-Host "Warning: File $file not found" -ForegroundColor Red
    }
}

# Step 3: Also add our compatibility header to EndpointsCommon directory
$endpointCommonPath = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\EndpointsCommon"
if (Test-Path $endpointCommonPath) {
    # Copy our compatibility header to EndpointsCommon directory
    $sourceHeader = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\audio_compat.h"
    $targetHeader = "$endpointCommonPath\audio_compat.h"
    Copy-Item -Path $sourceHeader -Destination $targetHeader -Force
    Write-Host "Copied audio_compat.h to EndpointsCommon directory" -ForegroundColor Green
    
    # Update wave table header files in EndpointsCommon
    $endpointFiles = @(
        "$endpointCommonPath\bthhfpmicwavtable.h",
        "$endpointCommonPath\usbhsmicwavtable.h"
    )
    
    foreach ($file in $endpointFiles) {
        if (Test-Path $file) {
            $content = Get-Content -Path $file -Raw
            if ($content -notmatch "audio_compat\.h") {
                $newContent = "// Include our audio driver compatibility header first`n#include `"audio_compat.h`"`n`n$content"
                Set-Content -Path $file -Value $newContent
                Write-Host "Updated $file to include audio_compat.h" -ForegroundColor Green
            }
        }
    }
}

# Step 4: Apply fixes to DataRangeIntersection function implementation
$fixSourceFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\fix_lamaloopbackrender.cpp"
$targetSourceFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp"

if (Test-Path $fixSourceFile -and Test-Path $targetSourceFile) {
    $fixContent = Get-Content -Path $fixSourceFile -Raw
    
    # Extract the fixed DataRangeIntersection implementation
    $pattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection[\s\S]*?return STATUS_SUCCESS;\s*\}"
    $match = [regex]::Match($fixContent, $pattern)
    
    if ($match.Success) {
        $targetContent = Get-Content -Path $targetSourceFile -Raw
        
        # Search for the original implementation to replace
        $targetPattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection[\s\S]*?return STATUS_SUCCESS;\s*\}"
        $targetMatch = [regex]::Match($targetContent, $targetPattern)
        
        if ($targetMatch.Success) {
            $newContent = $targetContent.Replace($targetMatch.Value, $match.Value)
            Set-Content -Path $targetSourceFile -Value $newContent
            Write-Host "Updated DataRangeIntersection implementation in lamaloopbackrender.cpp" -ForegroundColor Green
        } else {
            Write-Host "Could not find original DataRangeIntersection implementation in lamaloopbackrender.cpp" -ForegroundColor Yellow
        }
    } else {
        Write-Host "Could not find fixed DataRangeIntersection implementation in fix_lamaloopbackrender.cpp" -ForegroundColor Yellow
    }
}

# Step 5: Create a simple build batch file that uses our compatibility fix
$buildBatch = @"
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
"@

$buildBatchFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\build_with_audio_fix.bat"
Set-Content -Path $buildBatchFile -Value $buildBatch
Write-Host "Created build batch file with audio driver compatibility options" -ForegroundColor Green

# Step 6: Run the build batch file
Write-Host "Running the build batch file..." -ForegroundColor Yellow
& cmd.exe /c $buildBatchFile

Write-Host "Build process completed." -ForegroundColor Cyan
Write-Host "If the build succeeded, the audio driver compilation issues have been fixed!" -ForegroundColor Green
Write-Host "If errors persist, check the specific error messages for more targeted fixes." -ForegroundColor Yellow
