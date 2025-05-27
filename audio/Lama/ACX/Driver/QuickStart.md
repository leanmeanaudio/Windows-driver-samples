# LAMAConnect Virtual Audio Driver - Quick Start Guide

## 🚀 Get Up and Running in 5 Minutes

This guide will get you from source code to working virtual audio driver as quickly as possible.

## Prerequisites Checklist

- [ ] **Windows 10 Version 19041 or later** (check with `winver`)
- [ ] **Visual Studio 2019/2022** with C++ development tools
- [ ] **Windows Driver Kit (WDK) 10.0.26100 or later**
- [ ] **Administrator privileges**

## Step 1: Enable Test Signing (One-time setup)

```powershell
# Run PowerShell as Administrator
.\Build-LAMAConnect.ps1 -EnableTestSigning
# Reboot when prompted
```

## Step 2: Build and Install Driver

```powershell
# Run PowerShell as Administrator
.\Build-LAMAConnect.ps1 -Configuration Release -Install
```

That's it! The script will:
- ✅ Build the driver
- ✅ Uninstall any existing version
- ✅ Install the new driver
- ✅ Create virtual audio devices
- ✅ Verify installation

## Step 3: Test the Installation

### Check Device Manager
1. Open Device Manager (`devmgmt.msc`)
2. Look for **"LAMAConnect Virtual Audio Device"** under Sound devices
3. Status should be **"Working properly"**

### Check Audio Devices
1. Open Sound settings (`ms-settings:sound`)
2. You should see:
   - **LAMAConnect Virtual Speaker** (output device)
   - **LAMAConnect Virtual Microphone** (input device)

## Step 4: Build the JUCE Plugin (Optional)

```bash
# Open the JUCE .jucer file in Projucer
# Export to Visual Studio
# Build in Visual Studio or:
msbuild LAMAConnectPlugin.sln /p:Configuration=Release
```

## Quick Verification Test

### Test 1: Basic Driver Function
```powershell
# Check if driver service is running
Get-Service LAMAConnect
# Should show "Running"
```

### Test 2: Device Interface
```powershell
# Test driver communication
$handle = [System.IO.File]::Open("\\.\LAMAConnect0", "Open", "Read")
$handle.Close()
# Should complete without error
```

### Test 3: Audio Routing
1. Set **LAMAConnect Virtual Speaker** as default playback device
2. Set **LAMAConnect Virtual Microphone** as default recording device
3. Play audio - it should route through the virtual device

## File Structure Overview

After building, you'll have:

```
LAMAConnect/
├── x64/Release/
│   └── LAMAConnectDriver.sys    # Built driver binary
├── LAMAConnectDriver.c          # Main driver source
├── LAMAConnectDriver.h          # Driver headers
├── LAMAConnectShared.h          # Shared definitions
├── AudioCodec.inf               # Driver installation file
├── LAMAConnectInterface.cpp     # User-mode interface
├── PluginProcessor.cpp          # JUCE plugin
└── Build-LAMAConnect.ps1        # Build script
```

## Troubleshooting Quick Fixes

### "Build failed" Error
```powershell
# Clean and rebuild
.\Build-LAMAConnect.ps1 -Clean
.\Build-LAMAConnect.ps1 -Configuration Release -Install
```

### "Driver not found" Error
```powershell
# Check test signing
bcdedit /enum | findstr testsigning
# Should show "testsigning Yes"
```

### "Access denied" Error
```powershell
# Ensure running as Administrator
# Check if Windows Defender is blocking
```

### Plugin Can't Connect
1. **Check driver status**: `Get-Service LAMAConnect` should show "Running"
2. **Run plugin as admin**: Some DAWs need elevated privileges
3. **Check instance number**: Try driver instance 0 first

## Common Usage Patterns

### Pattern 1: Audio Loopback
- Route app audio to LAMAConnect Virtual Speaker
- Route LAMAConnect Virtual Microphone to recording app
- Audio flows: App → Virtual Speaker → Driver → Virtual Microphone → Recording App

### Pattern 2: Multi-Instance Setup
- Use different driver instances (0-3) for separate audio paths
- Each instance creates independent virtual devices
- Perfect for complex routing scenarios

### Pattern 3: Plugin Processing
- Load plugin in DAW
- Plugin processes audio through driver
- Apply effects/processing in real-time
- Route processed audio to other applications

## Advanced Options

### Multiple Driver Instances
```powershell
# Create multiple instances
devcon install AudioCodec.inf ROOT\LAMAConnect
# Creates LAMAConnect0, LAMAConnect1, etc.
```

### Debug Build
```powershell
.\Build-LAMAConnect.ps1 -Configuration Debug -Install
# Includes debug symbols and logging
```

### Uninstall
```powershell
.\Build-LAMAConnect.ps1 -Uninstall
# Completely removes driver and devices
```

## Next Steps

1. **Test with your DAW**: Load the plugin and verify audio routing
2. **Experiment with channel counts**: Try different channel configurations
3. **Multiple instances**: Create additional virtual devices for complex routing
4. **Production deployment**: Sign driver for distribution

## Getting Help

### Check Logs
- **Event Viewer**: Windows Logs → System (filter by LAMAConnect)
- **Device Manager**: Check device status and error codes
- **Debug Output**: Use DebugView for real-time driver logging

### Common Error Codes
- **Code 10**: Driver failed to start (check test signing)
- **Code 39**: Driver couldn't load (wrong architecture/dependencies)
- **Code 52**: Driver not signed (enable test signing)

### Debug Commands
```powershell
# Driver status
sc query LAMAConnect

# Device status  
pnputil /enum-devices | findstr LAMAConnect

# Shared memory check
Get-Process | Where-Object {$_.ProcessName -eq "audiodg"}
```

## Success! 🎉

If you've made it this far, you should have:
- ✅ Working virtual audio driver
- ✅ Virtual audio devices in Windows
- ✅ Driver interface accessible
- ✅ Ready for audio routing

You're now ready to route audio between applications using the LAMAConnect virtual audio driver!

---

**Need help?** Check the full `BUILD_INSTRUCTIONS.md` for detailed troubleshooting and advanced configuration options.