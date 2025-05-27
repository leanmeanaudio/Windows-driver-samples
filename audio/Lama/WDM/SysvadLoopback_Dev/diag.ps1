# Diagnostic script to find compiler errors
$ErrorLogFile = "build_errors.txt"

# Clean the project first
Write-Host "Cleaning project..."
& msbuild /t:Clean /p:Platform=Win32 /p:Configuration=Debug /v:quiet

# Redirect build output to file with detailed verbosity
Write-Host "Building project and logging errors to $ErrorLogFile..."
& msbuild /p:Platform=Win32 /p:Configuration=Debug /v:detailed > $ErrorLogFile 2>&1

# Look for error patterns in the output
Write-Host "Analyzing build errors..."
$ErrorCount = 0
$errors = Select-String -Path $ErrorLogFile -Pattern "error C\d+:"
foreach ($error in $errors) {
    Write-Host $error
    $ErrorCount++
}

Write-Host "Found $ErrorCount errors. See $ErrorLogFile for details."
