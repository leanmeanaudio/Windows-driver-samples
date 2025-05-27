#include <Windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <iostream>
#include <string>
#include <vector>
#include <codecvt>
#include <locale>
#include <conio.h>

// Convert wstring to string
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Class for COM initialization and cleanup
class ComInit {
public:
    ComInit() : m_initialized(false) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (SUCCEEDED(hr)) {
            m_initialized = true;
            std::cout << "[INFO] COM initialized successfully" << std::endl;
        }
        else {
            std::cerr << "[ERROR] Failed to initialize COM: 0x" << std::hex << hr << std::dec << std::endl;
        }
    }

    ~ComInit() {
        if (m_initialized) {
            CoUninitialize();
            std::cout << "[INFO] COM uninitialized" << std::endl;
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

// Struct to hold audio device information
struct AudioDeviceInfo {
    std::wstring id;
    std::wstring name;
    std::wstring description;
    bool isDefault;
    bool isLAMAConnectDevice;
};

// Enumerate audio devices and look for LAMAConnect driver
std::vector<AudioDeviceInfo> EnumerateAudioDevices(EDataFlow dataFlow, bool verbose) {
    std::vector<AudioDeviceInfo> devices;
    
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDeviceCollection* pCollection = NULL;
    IMMDevice* pDefaultDevice = NULL;
    std::wstring defaultDeviceId;
    
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to create device enumerator: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        return devices;
    }
    
    // Get the default device ID
    hr = pEnumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, &pDefaultDevice);
    if (SUCCEEDED(hr)) {
        LPWSTR pwszID = NULL;
        hr = pDefaultDevice->GetId(&pwszID);
        if (SUCCEEDED(hr)) {
            defaultDeviceId = pwszID;
            CoTaskMemFree(pwszID);
        }
        pDefaultDevice->Release();
    }
    
    // Enumerate all devices
    hr = pEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to enumerate audio endpoints: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        pEnumerator->Release();
        return devices;
    }
    
    UINT count;
    hr = pCollection->GetCount(&count);
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to get device count: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        pCollection->Release();
        pEnumerator->Release();
        return devices;
    }
    
    std::cout << "[INFO] Found " << count << " " 
              << (dataFlow == eRender ? "output" : "input") << " devices" << std::endl;
    
    for (UINT i = 0; i < count; i++) {
        IMMDevice* pDevice = NULL;
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr)) {
            std::cerr << "[ERROR] Failed to get device " << i << ": 0x" 
                      << std::hex << hr << std::dec << std::endl;
            continue;
        }
        
        AudioDeviceInfo deviceInfo;
        
        // Get device ID
        LPWSTR pwszID = NULL;
        hr = pDevice->GetId(&pwszID);
        if (SUCCEEDED(hr)) {
            deviceInfo.id = pwszID;
            deviceInfo.isDefault = (deviceInfo.id == defaultDeviceId);
            CoTaskMemFree(pwszID);
        }
        
        // Get device properties
        IPropertyStore* pProps = NULL;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (SUCCEEDED(hr)) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            
            hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
            if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
                deviceInfo.name = varName.pwszVal;
            }
            PropVariantClear(&varName);
            
            PROPVARIANT varDesc;
            PropVariantInit(&varDesc);
            
            hr = pProps->GetValue(PKEY_Device_DeviceDesc, &varDesc);
            if (SUCCEEDED(hr) && varDesc.vt == VT_LPWSTR) {
                deviceInfo.description = varDesc.pwszVal;
            }
            PropVariantClear(&varDesc);
            
            pProps->Release();
        }
        
        // Check if this is likely our LAMAConnect device
        deviceInfo.isLAMAConnectDevice = 
            deviceInfo.name.find(L"LAMAConnect") != std::wstring::npos || 
            deviceInfo.description.find(L"LAMAConnect") != std::wstring::npos;
        
        devices.push_back(deviceInfo);
        
        if (verbose) {
            std::cout << "---------------------------------------------" << std::endl;
            std::cout << "Device " << i << (deviceInfo.isDefault ? " (Default)" : "") << std::endl;
            std::cout << "ID: " << WideToUtf8(deviceInfo.id) << std::endl;
            std::cout << "Name: " << WideToUtf8(deviceInfo.name) << std::endl;
            std::cout << "Description: " << WideToUtf8(deviceInfo.description) << std::endl;
            std::cout << "Is LAMAConnect Device: " << (deviceInfo.isLAMAConnectDevice ? "Yes" : "No") << std::endl;
        }
        
        pDevice->Release();
    }
    
    pCollection->Release();
    pEnumerator->Release();
    
    return devices;
}

// Try to open the LAMAConnect audio device
bool TestAudioDeviceConnection(const AudioDeviceInfo& device, EDataFlow dataFlow, bool verbose) {
    if (verbose) {
        std::cout << "\n[INFO] Attempting to connect to: " << WideToUtf8(device.name) << std::endl;
    }
    
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    WAVEFORMATEX* pwfx = NULL;
    
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to create device enumerator: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }
    
    hr = pEnumerator->GetDevice(device.id.c_str(), &pDevice);
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to get device by ID: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        pEnumerator->Release();
        return false;
    }
    
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to activate audio client: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        pDevice->Release();
        pEnumerator->Release();
        return false;
    }
    
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to get mix format: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        return false;
    }
    
    if (verbose) {
        std::cout << "[INFO] Successfully retrieved format:" << std::endl;
        std::cout << "  Sample Rate: " << pwfx->nSamplesPerSec << " Hz" << std::endl;
        std::cout << "  Channels: " << pwfx->nChannels << std::endl;
        std::cout << "  Bits Per Sample: " << pwfx->wBitsPerSample << std::endl;
    }
    
    // Try to initialize the audio client (shared mode)
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,  // No flags
        0,  // Default buffer duration
        0,  // Periodicity
        pwfx,
        NULL);
    
    if (FAILED(hr)) {
        std::cerr << "[ERROR] Failed to initialize audio client: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        return false;
    }
    
    std::cout << "[SUCCESS] Successfully connected to audio device!" << std::endl;
    
    // Clean up
    CoTaskMemFree(pwfx);
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    
    return true;
}

int main() {
    std::cout << "===== LAMAConnect Audio Driver Test Application =====" << std::endl;
    std::cout << "This application will check if the LAMAConnect driver is installed" << std::endl;
    std::cout << "and attempt to connect to it for testing purposes." << std::endl;
    std::cout << "======================================================" << std::endl << std::endl;
    
    // Initialize COM
    ComInit comInit;
    if (!comInit.IsInitialized()) {
        std::cerr << "Failed to initialize COM. Exiting." << std::endl;
        return 1;
    }
    
    // Enumerate output (render) devices
    std::cout << "\n=== OUTPUT DEVICES ===" << std::endl;
    std::vector<AudioDeviceInfo> outputDevices = EnumerateAudioDevices(eRender, true);
    
    // Enumerate input (capture) devices
    std::cout << "\n=== INPUT DEVICES ===" << std::endl;
    std::vector<AudioDeviceInfo> inputDevices = EnumerateAudioDevices(eCapture, true);
    
    // Check if LAMAConnect devices were found
    std::vector<AudioDeviceInfo> lamaOutputDevices;
    std::vector<AudioDeviceInfo> lamaInputDevices;
    
    for (const auto& device : outputDevices) {
        if (device.isLAMAConnectDevice) {
            lamaOutputDevices.push_back(device);
        }
    }
    
    for (const auto& device : inputDevices) {
        if (device.isLAMAConnectDevice) {
            lamaInputDevices.push_back(device);
        }
    }
    
    std::cout << "\n=== LAMA CONNECT DRIVER STATUS ===" << std::endl;
    
    if (lamaOutputDevices.empty() && lamaInputDevices.empty()) {
        std::cout << "[WARNING] No LAMAConnect devices were found!" << std::endl;
        std::cout << "The driver may not be installed or may not be functioning correctly." << std::endl;
    } else {
        std::cout << "[INFO] Found " << lamaOutputDevices.size() << " LAMAConnect output device(s)" << std::endl;
        std::cout << "[INFO] Found " << lamaInputDevices.size() << " LAMAConnect input device(s)" << std::endl;
        
        // Test connecting to the devices
        std::cout << "\n=== TESTING CONNECTIONS ===" << std::endl;
        
        for (const auto& device : lamaOutputDevices) {
            bool success = TestAudioDeviceConnection(device, eRender, true);
            std::cout << "Output Device '" << WideToUtf8(device.name) << "': " 
                     << (success ? "CONNECTED" : "FAILED") << std::endl;
        }
        
        for (const auto& device : lamaInputDevices) {
            bool success = TestAudioDeviceConnection(device, eCapture, true);
            std::cout << "Input Device '" << WideToUtf8(device.name) << "': " 
                     << (success ? "CONNECTED" : "FAILED") << std::endl;
        }
    }
    
    std::cout << "\nPress any key to exit..." << std::endl;
    _getch();
    
    return 0;
}
