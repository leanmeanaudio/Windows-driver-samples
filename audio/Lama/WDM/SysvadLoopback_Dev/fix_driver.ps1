# fix_driver.ps1
# Direct approach to fixing Windows audio driver compilation issues

Write-Host "Applying direct fixes to SysvadLoopback_Dev driver..." -ForegroundColor Cyan

# Define the list of files to update with our compatibility header
$filesToUpdate = @(
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\micintopo.cpp",
    "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\hdmitopo.cpp"
)

# Add our direct fix header to the beginning of each file
foreach ($file in $filesToUpdate) {
    if (Test-Path $file) {
        $content = Get-Content -Path $file -Raw
        $newContent = "// Include our direct fix header at the very beginning`n#include `"direct_fix.h`"`n`n$content"
        Set-Content -Path $file -Value $newContent
        Write-Host "Updated $file to include direct_fix.h at the beginning" -ForegroundColor Green
    } else {
        Write-Host "Warning: File $file not found" -ForegroundColor Yellow
    }
}

# Create a simpler build batch file that doesn't use force-include
$buildBatch = @"
@echo off
echo Building SysvadLoopback_Dev driver with basic compatibility options...

:: Set basic compiler options
set CL=/DWINDOWS_DRIVER /D_WINDLL /DDRIVER_FIXED

:: Run MSBuild with simple options
msbuild /p:Platform=x64 /p:Configuration=Debug /v:minimal

:: Check the build result
if %ERRORLEVEL% == 0 (
  echo Build completed successfully!
) else (
  echo Build failed with error level %ERRORLEVEL%
)
"@

$buildBatchFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\basic_build.bat"
Set-Content -Path $buildBatchFile -Value $buildBatch
Write-Host "Created basic build batch file" -ForegroundColor Green

# Run the build batch file
Write-Host "Running the build batch file..." -ForegroundColor Yellow
& cmd.exe /c $buildBatchFile

Write-Host "Build process completed." -ForegroundColor Cyan
