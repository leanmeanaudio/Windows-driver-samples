# PowerShell script to patch the lamaloopbackrender.cpp file

$targetFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\lamaloopbackrender.cpp"
$fixFile = "C:\Dev\Windows Drivers Examples\audio\Lama\SysvadLoopback_Dev\fix_render.cpp"

# Read the fixed implementation
$fixedCode = Get-Content $fixFile -Raw

# Read the original file
$originalCode = Get-Content $targetFile -Raw

# Create backup of original file
Copy-Item $targetFile "$targetFile.bak" -Force
Write-Host "Created backup at $targetFile.bak"

# Extract the fixed functions from fix_render.cpp
$dataRangeIntersectionPattern = "NTSTATUS CMiniportTopologyLamaLoopbackRender::DataRangeIntersection.*?return STATUS_SUCCESS;\r?\n}" -replace "\(", "\(" -replace "\)", "\)" -replace "\[", "\[" -replace "\]", "\]"
$waveRTDataRangeIntersectionPattern = "NTSTATUS CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection.*?return STATUS_SUCCESS;\r?\n}" -replace "\(", "\(" -replace "\)", "\)" -replace "\[", "\[" -replace "\]", "\]"

if ($fixedCode -match $dataRangeIntersectionPattern) {
    $fixedTopoFunc = $matches[0]
    Write-Host "Found fixed CMiniportTopologyLamaLoopbackRender::DataRangeIntersection"
} else {
    Write-Host "Could not find CMiniportTopologyLamaLoopbackRender::DataRangeIntersection in fix file" -ForegroundColor Red
    exit 1
}

if ($fixedCode -match $waveRTDataRangeIntersectionPattern) {
    $fixedWaveRTFunc = $matches[0]
    Write-Host "Found fixed CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection"
} else {
    Write-Host "Could not find CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection in fix file" -ForegroundColor Red
    exit 1
}

# Replace the functions in the original file
$originalDataRangeIntersectionPattern = "NTSTATUS\s+CMiniportTopologyLamaLoopbackRender::DataRangeIntersection.*?return STATUS_SUCCESS;\r?\n}" -replace "\(", "\(" -replace "\)", "\)" -replace "\[", "\[" -replace "\]", "\]"
$originalWaveRTDataRangeIntersectionPattern = "NTSTATUS\s+CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection.*?return STATUS_SUCCESS;\r?\n}" -replace "\(", "\(" -replace "\)", "\)" -replace "\[", "\[" -replace "\]", "\]"

if ($originalCode -match $originalDataRangeIntersectionPattern) {
    Write-Host "Found original CMiniportTopologyLamaLoopbackRender::DataRangeIntersection to replace"
    $patchedCode = $originalCode -replace $originalDataRangeIntersectionPattern, $fixedTopoFunc
} else {
    Write-Host "Could not find original CMiniportTopologyLamaLoopbackRender::DataRangeIntersection" -ForegroundColor Red
    exit 1
}

if ($patchedCode -match $originalWaveRTDataRangeIntersectionPattern) {
    Write-Host "Found original CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection to replace"
    $patchedCode = $patchedCode -replace $originalWaveRTDataRangeIntersectionPattern, $fixedWaveRTFunc
} else {
    Write-Host "Could not find original CMiniportWaveRTLamaLoopbackRender::DataRangeIntersection" -ForegroundColor Red
    exit 1
}

# Save the patched file
Set-Content -Path $targetFile -Value $patchedCode
Write-Host "Successfully patched $targetFile with fixed DataRangeIntersection functions"
