# apply_waveformat_fix.ps1
# Targeted approach to fix WAVEFORMATEXTENSIBLE compatibility issues

Write-Host "Applying specialized WAVEFORMATEXTENSIBLE fix to SysvadLoopback_Dev driver..." -ForegroundColor Cyan

# Define the list of key files to update
$filesToUpdate = @(
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackcommon.h"
)

# Add our waveformat fix header to each file
foreach ($file in $filesToUpdate) {
    if (Test-Path $file) {
        $content = Get-Content -Path $file -Raw
        
        # For header files, add our include at the very beginning
        if ($file.EndsWith(".h")) {
            $newContent = "#include `"waveformat_fix.h`"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility`n`n$content"
        }
        # For cpp files, add after any #include statements
        else {
            if ($content -match "(?ms)^(.*?#include.*?)(\r?\n\r?\n)(.*)$") {
                $newContent = $matches[1] + "`n#include `"waveformat_fix.h`"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility" + $matches[2] + $matches[3]
            } else {
                # If pattern not matched, just prepend
                $newContent = "#include `"waveformat_fix.h`"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility`n`n$content"
            }
        }
        
        Set-Content -Path $file -Value $newContent
        Write-Host "Updated $file to include waveformat_fix.h" -ForegroundColor Green
    } else {
        Write-Host "Warning: File $file not found" -ForegroundColor Yellow
    }
}

# Also create a stripped-down version for the EndpointsCommon directory
$endpointCommonPath = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\EndpointsCommon"
if (Test-Path $endpointCommonPath) {
    $waveformatFixPath = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\waveformat_fix.h"
    $targetPath = "$endpointCommonPath\waveformat_fix.h"
    
    # Copy the file
    Copy-Item -Path $waveformatFixPath -Destination $targetPath
    Write-Host "Copied waveformat_fix.h to EndpointsCommon directory" -ForegroundColor Green
    
    # Update the bthhfpmicwavtable.h and usbhsmicwavtable.h files
    $endpointFiles = @(
        "$endpointCommonPath\bthhfpmicwavtable.h",
        "$endpointCommonPath\usbhsmicwavtable.h"
    )
    
    foreach ($file in $endpointFiles) {
        if (Test-Path $file) {
            $content = Get-Content -Path $file -Raw
            $newContent = "#include `"waveformat_fix.h`"  // Fix for WAVEFORMATEXTENSIBLE structure compatibility`n`n$content"
            Set-Content -Path $file -Value $newContent
            Write-Host "Updated $file to include waveformat_fix.h" -ForegroundColor Green
        }
    }
}

# Create a build batch file with minimal compiler flags
$buildBatch = @"
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
"@

$buildBatchFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\build_with_waveformat_fix.bat"
Set-Content -Path $buildBatchFile -Value $buildBatch
Write-Host "Created build batch file with WAVEFORMATEXTENSIBLE compatibility options" -ForegroundColor Green

# Run the build batch file
Write-Host "Running the build batch file..." -ForegroundColor Yellow
& cmd.exe /c $buildBatchFile

Write-Host "Build process completed." -ForegroundColor Cyan
Write-Host "If build errors persist, examine the output for specific structure definition issues." -ForegroundColor Yellow
