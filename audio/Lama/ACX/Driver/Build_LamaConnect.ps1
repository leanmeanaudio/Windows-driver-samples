# LAMAConnect Driver Build and Installation Script
# Run as Administrator

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",
    
    [Parameter(Mandatory=$false)]
    [switch]$Install,
    
    [Parameter(Mandatory=$false)]
    [switch]$Uninstall,
    
    [Parameter(Mandatory=$false)]
    [switch]$EnableTestSigning,
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean
)

# Script configuration
$ErrorActionPreference = "Stop"
$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectPath = Join-Path $ScriptPath "AudioCodec.sln"
$DriverName = "LAMAConnect"
$DeviceHardwareId = "ROOT\LAMAConnect"

# Color output functions
function Write-ColorOutput([ConsoleColor]$ForegroundColor, [string]$Message) {
    $currentForeground = $host.UI.RawUI.ForegroundColor
    $host.UI.RawUI.ForegroundColor = $ForegroundColor
    Write-Output $Message
    $host.UI.RawUI.ForegroundColor = $currentForeground
}

function Write-Success([string]$Message) { Write-Host "SUCCESS: $Message" }
function Write-Warning([string]$Message) { Write-Host "WARNING: $Message" }
function Write-Error([string]$Message) { Write-Host "ERROR: $Message" }
function Write-Info([string]$Message) { Write-Host "INFO: $Message" }

# Check if running as administrator
function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Enable test signing
function Enable-TestSigning {
    Write-Info "Enabling test signing..."
    try {
        $result = bcdedit /set testsigning on
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Test signing enabled. Reboot required to take effect."
            Write-Warning "Please reboot your system and run this script again."
            return $true
        } else {
            Write-Error "Failed to enable test signing: $result"
            return $false
        }
    } catch {
        Write-Error "Error enabling test signing: $_"
        return $false
    }
}

# Check if test signing is enabled
function Test-TestSigning {
    try {
        $result = bcdedit /enum | Select-String "testsigning"
        if ($result -and $result.ToString().Contains("Yes")) {
            return $true
        }
        return $false
    } catch {
        return $false
    }
}

# Find Visual Studio build tools
function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsPath) {
            $MsBuildPathX64 = Join-Path $vsPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
            $MsBuildPathDefault = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"

            if (Test-Path $MsBuildPathX64) {
                $MsBuildPath = $MsBuildPathX64
                Write-Host "INFO: Using amd64 MSBuild: $MsBuildPath" -ForegroundColor Cyan
            } else {
                $MsBuildPath = $MsBuildPathDefault
                Write-Host "INFO: Using default MSBuild: $MsBuildPath" -ForegroundColor Cyan
            }

            return $MsBuildPath
        }
    }
    
    # Fallback to PATH
    try {
        $msbuildPath = (Get-Command msbuild -ErrorAction Stop).Source
        return $msbuildPath
    } catch {
        return $null
    }
}

# Clean build outputs
function Invoke-Clean {
    Write-Info "Cleaning build outputs..."
    
    $cleanPaths = @(
        "x64",
        "ARM64",
        "Debug",
        "Release",
        "*.log",
        "*.wrn"
    )
    
    foreach ($path in $cleanPaths) {
        $fullPath = Join-Path $ScriptPath $path
        if (Test-Path $fullPath) {
            Remove-Item $fullPath -Recurse -Force
            Write-Success "Removed $path"
        }
    }
}

# Build the driver
function Invoke-Build {
    Write-Info "Building LAMAConnect driver ($($Configuration)|$($Platform))..."
    
    $msbuild = Find-MSBuild
    if (-not $msbuild) {
        Write-Error "MSBuild not found. Please install Visual Studio 2019/2022 with C++ development tools."
        return $false
    }
    
    Write-Info "Using MSBuild: $msbuild"
    
    $buildArgs = @(
        $ProjectPath,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/p:TargetVersion=Windows10",
        "/p:KMDF_VERSION_MAJOR=1",
        "/p:KMDF_VERSION_MINOR=31",
        "/p:ACX_VERSION_MAJOR=1",
        "/p:ACX_VERSION_MINOR=1",
        "/p:PreferredToolArchitecture=x64",
        "/m",
        "/v:minimal"
    )
    
    try {
        & $msbuild @buildArgs
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Build completed successfully"
            return $true
        } else {
            Write-Error "Build failed with exit code $LASTEXITCODE"
            return $false
        }
    } catch {
        Write-Error "Build error: $_"
        return $false
    }
}

# Check if driver is installed
function Test-DriverInstalled {
    try {
        $service = Get-Service -Name $DriverName -ErrorAction SilentlyContinue
        return $service -ne $null
    } catch {
        return $false
    }
}

# Uninstall existing driver
function Invoke-Uninstall {
    Write-Info "Uninstalling existing LAMAConnect driver..."
    
    # Stop service if running
    try {
        $service = Get-Service -Name $DriverName -ErrorAction SilentlyContinue
        if ($service -and $service.Status -eq "Running") {
            Write-Info "Stopping driver service..."
            Stop-Service -Name $DriverName -Force
            Write-Success "Driver service stopped"
        }
    } catch {
        Write-Warning "Could not stop driver service: $_"
    }
    
    # Remove device
    try {
        $devcon = "${env:ProgramFiles(x86)}\Windows Kits\10\Tools\x64\devcon.exe"
        if (Test-Path $devcon) {
            Write-Info "Removing device instances..."
            & $devcon remove $DeviceHardwareId
        }
    } catch {
        Write-Warning "Could not remove device instances: $_"
    }
    
    # Remove driver package
    try {
        Write-Info "Removing driver package..."
        pnputil /delete-driver AudioCodec.inf /uninstall /force
        Write-Success "Driver package removed"
    } catch {
        Write-Warning "Could not remove driver package: $_"
    }
}

# Install the driver
function Invoke-Install {
    param([string]$BuildPath)
    
    Write-Info "Installing LAMAConnect driver..."
    
    # Create temporary installation directory
    $tempDir = Join-Path $env:TEMP "LAMAConnect_Install"
    if (Test-Path $tempDir) {
        Remove-Item $tempDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $tempDir | Out-Null
    
    # Copy files
    $driverSys = Join-Path $BuildPath "LAMAConnectDriver.sys"
    $driverInf = Join-Path $ScriptPath "AudioCodec.inf"
    
    if (-not (Test-Path $driverSys)) {
        Write-Error "Driver binary not found: $driverSys"
        return $false
    }
    
    if (-not (Test-Path $driverInf)) {
        Write-Error "Driver INF not found: $driverInf"
        return $false
    }
    
    Copy-Item $driverSys $tempDir
    Copy-Item $driverInf $tempDir
    
    # Install driver
    try {
        Write-Info "Adding driver package..."
        Push-Location $tempDir
        
        pnputil /add-driver AudioCodec.inf /install
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to add driver package"
            return $false
        }
        Write-Success "Driver package added"
        
        # Create device instance
        Write-Info "Creating device instance..."
        $devcon = "${env:ProgramFiles(x86)}\Windows Kits\10\Tools\x64\devcon.exe"
        if (Test-Path $devcon) {
            & $devcon install AudioCodec.inf $DeviceHardwareId
            if ($LASTEXITCODE -eq 0) {
                Write-Success "Device instance created"
            } else {
                Write-Warning "Device instance creation failed, but driver package is installed"
            }
        } else {
            Write-Warning "DevCon not found. Device instance not created automatically."
            Write-Info "You can create it manually in Device Manager:"
            Write-Info "1. Open Device Manager"
            Write-Info "2. Action > Add legacy hardware"
            Write-Info "3. Select LAMAConnect Virtual Audio Device"
        }
        
        return $true
        
    } catch {
        Write-Error "Installation error: $_"
        return $false
    } finally {
        Pop-Location
        Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Verify installation
function Test-Installation {
    Write-Info "Verifying installation..."
    
    # Check service
    $service = Get-Service -Name $DriverName -ErrorAction SilentlyContinue
    if ($service) {
        Write-Success "Driver service found: $($service.Status)"
    } else {
        Write-Warning "Driver service not found"
    }
    
    # Check device
    $device = Get-PnpDevice | Where-Object { $_.InstanceId -like "*LAMAConnect*" }
    if ($device) {
        Write-Success "Device found: $($device.FriendlyName) ($($device.Status))"
    } else {
        Write-Warning "Device not found in Device Manager"
    }
    
    # Check shared memory (if driver is running)
    try {
        $handle = [System.IO.File]::Open("\\.\LAMAConnect0", "Open", "Read", "ReadWrite")
        $handle.Close()
        Write-Success "Driver interface accessible"
    } catch {
        Write-Warning "Driver interface not accessible (driver may not be running)"
    }
}

# Main execution
function Main {
    Write-Info "LAMAConnect Driver Build Script"
    Write-Info "=============================="
    
    # Check administrator privileges
    if (-not (Test-Administrator)) {
        Write-Warning "This script requires administrator privileges. Please re-run as administrator."
        # exit 1 # Exiting here might close the console window immediately.
        # Consider prompting to re-launch as admin or just warning and continuing if some operations don't strictly need admin.
    }

    if ($EnableTestSigning) {
        if (-not (Test-TestSigning)) {
            if (-not (Enable-TestSigning)) {
                exit 1
            }
            # If test signing was just enabled, it often requires a reboot.
            # The Enable-TestSigning function should inform the user.
            # Depending on script's design, you might want to exit here or give instructions.
        } else {
            Write-Info "Test signing is already enabled."
        }
    }

    if ($Clean) {
        Invoke-Clean
    }
    
    if ($Uninstall) {
        Invoke-Uninstall
    }

    if (-not (Invoke-Build)) {
        Write-Error "Build process failed."
        exit 1
    }

    if ($Install) {
        if (-not (Test-TestSigning)) {
            Write-Warning "Test signing is not enabled. Driver installation might fail or require a reboot after enabling it."
            Write-Warning "Consider running with -EnableTestSigning parameter first and rebooting if prompted."
            # Optionally, you could attempt to enable it here, or exit.
        }
        Invoke-Install
    }

    Write-Success "Script completed successfully"
}

Main