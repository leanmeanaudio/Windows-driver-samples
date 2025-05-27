# apply_all_fixes.ps1
# This script applies all compatibility fixes to the SysvadLoopback_Dev driver

Write-Host "Applying all fixes to SysvadLoopback_Dev driver..." -ForegroundColor Cyan

# 1. Update include statements in source files to use the compatibility headers
$sourceFiles = @(
    "micintopo.cpp",
    "lamaloopbackrender.cpp"
)

foreach ($file in $sourceFiles) {
    $content = Get-Content -Path $file -Raw
    $newContent = $content -replace "// Include our compatibility fix header first\s+#include `"fix_ks_cpp.h`"", "// Include our unified compatibility header first`n#include `"sysvad_compat.h`""
    Set-Content -Path $file -Value $newContent
    Write-Host "Updated include statements in $file" -ForegroundColor Green
}

# 2. Replace the DataRangeIntersection function in lamaloopbackrender.cpp with the fixed version
$fixFile = Get-Content -Path "fix_lamaloopbackrender.cpp" -Raw
$targetFile = Get-Content -Path "lamaloopbackrender.cpp" -Raw

# Extract the fixed DataRangeIntersection implementation
$pattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection[\s\S]*?return STATUS_SUCCESS;\s*\}"
$fixedFunction = [regex]::Match($fixFile, $pattern).Value

# Replace the original implementation with the fixed one
$pattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection[\s\S]*?return STATUS_SUCCESS;\s*\}"
$targetFile = [regex]::Replace($targetFile, $pattern, $fixedFunction)

# Save the updated file
Set-Content -Path "lamaloopbackrender.cpp" -Value $targetFile
Write-Host "Updated DataRangeIntersection function in lamaloopbackrender.cpp" -ForegroundColor Green

# 3. Add compiler preprocessor definitions to handle compatibility issues
$env:CL = "/D_WINDLL /DDRIVER_FIXED /DKSMEDIA_FIXED_INCLUDED /DDRIVER_COMPATIBILITY_MODE"

# 4. Now build the project
Write-Host "Building the driver..." -ForegroundColor Yellow
msbuild /p:Platform=x64 /p:Configuration=Debug /v:normal

# Check build result
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host "Please check the error messages above for details." -ForegroundColor Yellow
}
