# LAMAConnect Driver Build-Only Script

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

# Script configuration
$ErrorActionPreference = "Stop"
$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectPath = Join-Path $ScriptPath "AudioCodec.sln"

# Output functions (simplified)
function Write-Success([string]$Message) { Write-Host "SUCCESS: $Message" }
function Write-Warning([string]$Message) { Write-Host "WARNING: $Message" }
function Write-Error([string]$Message) { Write-Host "ERROR: $Message" }
function Write-Info([string]$Message) { Write-Host "INFO: $Message" }

# Find Visual Studio build tools
function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            # Construct the path to MSBuild
            # Try to use amd64 version of MSBuild first
            $MsBuildPathX64 = Join-Path $vsPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
            $MsBuildPathDefault = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"

            if (Test-Path $MsBuildPathX64) {
                $MsBuildPath = $MsBuildPathX64
                Write-Host "INFO: Using amd64 MSBuild: $MsBuildPath"
            } else {
                $MsBuildPath = $MsBuildPathDefault
                Write-Host "INFO: Using default MSBuild: $MsBuildPath"
            }

            if (Test-Path $MsBuildPath) {
                return $MsBuildPath
            }
        }
    }
    
    # Fallback to PATH
    try {
        $msbuildPath = (Get-Command msbuild -ErrorAction Stop).Source
        return $msbuildPath
    } catch {
        Write-Warning "MSBuild not found via vswhere or PATH. Please ensure Visual Studio Build Tools are installed and in PATH."
        return $null
    }
}

# Build the driver
function Invoke-Build {
    Write-Info "Building LAMAConnect driver ($($Configuration)|$($Platform))..."
    
    $msbuildExePath = Find-MSBuild
    if (-not $msbuildExePath) {
        Write-Error "MSBuild executable not found. Cannot proceed with build."
        return $false
    }
    
    Write-Info "Using MSBuild: $msbuildExePath"
    
    $buildArgs = @(
        $ProjectPath,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/p:TargetVersion=Windows10", # Assuming default, adjust if needed
        "/p:KMDF_VERSION_MAJOR=1",    # Assuming default, adjust if needed
        "/p:KMDF_VERSION_MINOR=33",   # Changed from 31 to match vcxproj
        "/p:ACX_VERSION_MAJOR=1",     # Assuming default, adjust if needed
        "/p:ACX_VERSION_MINOR=1",     # Assuming default, adjust if needed
        "/p:PreferredToolArchitecture=x64", # Ensure x64 tools are preferred
        "/m",
        "/v:normal" # Use 'detailed' or 'diagnostic' for more verbose build logs if needed
    )
    
    try {
        Write-Host "DEBUG: Executing MSBuild..."
        $logFilePath = Join-Path $ScriptPath "msbuild_build.log"
        Write-Host "DEBUG: MSBuild output will be logged to: $logFilePath"
        if (Test-Path $logFilePath) {
            Remove-Item $logFilePath
            Write-Host "DEBUG: Removed old log file: $logFilePath"
        }
        # Redirect ALL output streams from MSBuild to the log file
        & $msbuildExePath @buildArgs *> "$logFilePath"

        $currentExitCode = $LASTEXITCODE # Store immediately
        Write-Host "DEBUG: MSBuild command finished. Stored LASTEXITCODE = $currentExitCode"

        if ($currentExitCode -eq 0) {
            Write-Success "Invoke-Build: MSBuild completed successfully (exit code: $currentExitCode)."
            return $true
        } else {
            Write-Error "Invoke-Build: MSBuild failed (exit code: $currentExitCode). Log is in $logFilePath"
            return $false
        }
    } catch {
        $exceptionMessage = $_.Exception.Message
        Write-Error "Invoke-Build: An exception occurred while trying to run MSBuild: $exceptionMessage"
        Write-Host "DEBUG: Exception caught in Invoke-Build. Current LASTEXITCODE (after error) = $LASTEXITCODE"
        return $false
    }
}

# --- Main execution ---
Write-Info "LAMAConnect Driver Build-Only Script started."
Write-Info "Configuration: $Configuration, Platform: $Platform"

$buildOutcome = Invoke-Build
Write-Host "DEBUG: Invoke-Build function returned: '$buildOutcome' (Type: $($buildOutcome.GetType().Name))"

if ($buildOutcome -eq $true) {
    Write-Success "Main script: Build process reported SUCCESS."
} else {
    Write-Error "Main script: Build process reported FAILURE. MSBuild issues detected."
    Write-Host "Full MSBuild output is in msbuild_build.log in the script directory."
    Write-Host "Script will now exit with code 1 to indicate failure."
    exit 1 
}

Write-Info "LAMAConnect Driver Build-Only Script finished (this message should only appear if build succeeded)."
