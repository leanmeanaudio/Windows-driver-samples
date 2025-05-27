# LAMAConnect Virtual Audio Driver - Build Instructions

## Prerequisites

### Required Software
1. **Visual Studio 2019 or 2022** with C++ development tools
2. **Windows Driver Kit (WDK) 10.0.26100 or later**
   - Download from: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
3. **Windows SDK 10.0.26100 or later** (usually included with WDK)

### System Requirements
- **Windows 10 Version 19041 (May 2020 Update) or later**
- **Administrator privileges** for driver installation
- **Test signing enabled** for development (see below)

## Build Steps

### 1. Enable Test Signing (Development Only)
```bash
# Open Command Prompt as Administrator
bcdedit /set testsigning on
# Reboot required
shutdown /r /t 0
```

### 2. Build the Driver
```bash
# Open Developer Command Prompt for VS 2022
# Navigate to driver source directory
cd C:\path\to\LAMAConnect\driver

# Build Debug version
msbuild AudioCodec.sln /p:Configuration=Debug /p:Platform=x64

# Build Release version
msbuild AudioCodec.sln /p:Configuration=Release /p:Platform=x64
```

### 3. Install the Driver
```bash
# Copy driver files to temp directory
mkdir C:\temp\LAMAConnect
copy x64\Debug\LAMAConnectDriver.sys C:\temp\LAMAConnect\
copy AudioCodec.inf C:\temp\LAMAConnect\

# Install driver
cd C:\temp\LAMAConnect
pnputil /add-driver AudioCodec.inf /install

# Create device instance
devcon install AudioCodec.inf ROOT\LAMAConnect
```

### 4. Verify Installation
```bash
# Check if driver is loaded
sc query LAMAConnect

# Check device manager for "LAMAConnect Virtual Audio Device"
devmgmt.msc
```

## Build the JUCE Plugin

### Prerequisites
- **JUCE Framework 7.0 or later**
- **Visual Studio 2019/2022** with C++ development

### Build Steps
1. Open the JUCE .jucer file
2. Configure target formats (VST3, AU, AAX as needed)
3. Export to Visual Studio
4. Build in Visual Studio

## Testing

### 1. Basic Driver Test
```bash
# Check if shared memory is created
# Should see LAMAConnectSharedMemory0 in Process Explorer

# Test with plugin in a DAW
# 1. Load plugin in DAW
# 2. Click "Initialize Driver"
# 3. Start audio playback
# 4. Check level meters for activity
```

### 2. Multiple Instance Test
```bash
# Load multiple plugin instances
# Switch to different driver instances (0-3)
# Each should create separate virtual audio devices
```

### 3. Audio Routing Test
```bash
# Route DAW output to LAMAConnect Virtual Speaker
# Route LAMAConnect Virtual Microphone to DAW input
# Test audio loopback functionality
```

## Troubleshooting

### Driver Won't Load
- **Check test signing**: `bcdedit /enum` should show testsigning Yes
- **Check driver signature**: Driver must be signed for production
- **Check WDK version**: Must be 10.0.26100 or later for ACX support
- **Check Event Viewer**: Look for driver loading errors

### Plugin Can't Connect
- **Check driver status**: `sc query LAMAConnect`
- **Check device manager**: Should see virtual audio device
- **Run as admin**: Plugin may need elevated privileges
- **Check shared memory**: Should see LAMAConnectSharedMemory* objects

### Audio Issues
- **Check sample rates**: Plugin and DAW must match
- **Check buffer sizes**: Use power-of-2 sizes (256, 512, 1024)
- **Check exclusive mode**: Disable WASAPI exclusive mode in Windows
- **Check audio enhancements**: Disable Windows audio enhancements

### Build Errors
- **"acx.h not found"**: Install/update WDK
- **"ACXDATAFORMAT_CONFIG undefined"**: API version mismatch, check ACX version
- **Linker errors**: Check library paths in project settings
- **"Cannot find WindowsKernelModeDriver10.0"**: Install WDK platform toolset

## Advanced Configuration

### Driver Signing for Production
```bash
# Get EV code signing certificate
# Sign driver with certificate
signtool sign /f certificate.p12 /p password /tr http://timestamp.digicert.com LAMAConnectDriver.sys

# Create catalog file
inf2cat /driver:. /os:10_X64

# Sign catalog
signtool sign /f certificate.p12 /p password /tr http://timestamp.digicert.com LAMAConnectDriver.cat
```

### Multiple Driver Instances
The driver supports up to 4 instances (0-3). Each instance creates:
- Separate shared memory: `LAMAConnectSharedMemory0`, `LAMAConnectSharedMemory1`, etc.
- Separate device interfaces: `\\.\LAMAConnect0`, `\\.\LAMAConnect1`, etc.
- Independent audio endpoints in Windows Audio

### Performance Tuning
- **Buffer Size**: Start with 512 frames, adjust based on latency requirements
- **Sample Rate**: 48kHz recommended for best compatibility
- **Channels**: Use only needed channels to reduce CPU usage
- **Priority**: Set audio thread priority to high for real-time performance

## File Structure
```
LAMAConnect/
├── driver/
│   ├── LAMAConnectDriver.c     # Main driver implementation
│   ├── LAMAConnectDriver.h     # Driver header
│   ├── LAMAConnectShared.h     # Shared definitions
│   ├── AudioCodec.inf          # Driver installation file
│   ├── AudioCodec.vcxproj      # Visual Studio project
│   ├── AudioCodec.sln          # Visual Studio solution
│   └── AudioCodec.vcxproj.Filters
├── interface/
│   ├── LAMAConnectInterface.cpp # User-mode interface
│   └── LAMAConnectInterface.h
├── plugin/
│   ├── PluginProcessor.cpp     # JUCE plugin processor
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp        # JUCE plugin editor
│   └── PluginEditor.h
└── docs/
    └── BUILD_INSTRUCTIONS.md   # This file
```

## Production Deployment

### Driver Package
1. **Sign all files** with EV certificate
2. **Create installer** with proper WiX/InstallShield
3. **Test on clean systems** without development tools
4. **Submit to Microsoft** for WHQL certification (optional but recommended)

### Plugin Package
1. **Sign plugin DLL** with code signing certificate
2. **Create installer** for common plugin directories
3. **Include redistributables** (Visual C++ Runtime)
4. **Test in multiple DAWs** (Pro Tools, Logic, Cubase, etc.)

## Support

For issues and questions:
- Check Event Viewer for driver errors
- Enable driver verifier for detailed debugging
- Use WinDbg for kernel debugging
- Check Windows Audio logs in Event Viewer

## Version History

- **v1.0.0**: Initial release with ACX support
- Support for 16-channel audio
- Multiple driver instances
- JUCE plugin interface
- Real-time processing with shared memory