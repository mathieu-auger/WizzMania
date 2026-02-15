#include "../include/HttpClient.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// HttpClient realisation 
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

HttpClient::HttpClient(const std::string& host, int port) 
    : serverHost(host), serverPort(port) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
}

HttpClient::~HttpClient() {
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string HttpClient::get(const std::string& path) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "ERROR: Cannot create socket";
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    
    struct hostent* host = gethostbyname(serverHost.c_str());
    if (!host) {
        closesocket(sock);
        return "ERROR: Cannot resolve hostname";
    }
    
    memcpy(&serverAddr.sin_addr, host->h_addr, host->h_length);
    
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        closesocket(sock);
        return "ERROR: Cannot connect to server";
    }
    
    std::string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: " + serverHost + "\r\n"
                          "Connection: close\r\n"
                          "\r\n";
    
    send(sock, request.c_str(), request.length(), 0);
    
    char buffer[4096] = {0};
    std::string response;
    int bytesRead;
    
    while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    
    closesocket(sock);
    
    size_t bodyPos = response.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        return response.substr(bodyPos + 4);
    }
    
    return response;
}

std::string HttpClient::ping() {
    return get("/ping");
}

std::string HttpClient::kiki() {
    return get("/kiki");
}

bool HttpClient::isServerAvailable() {
    std::string response = ping();
    return response.find("pong") != std::string::npos;
}

// Головна функція
void showMenu() {
    std::cout << "\n=== Client WIZZMANIA. Console version ===" << std::endl;
    std::cout << "Server: http://127.0.0.1:18080" << std::endl;
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  ping      - test /ping" << std::endl;
    std::cout << "  kiki      - test /kiki" << std::endl;
    std::cout << "  check     - client check" << std::endl;
    std::cout << "  help      - this is menu" << std::endl;
    std::cout << "  quit      - exit" << std::endl;
    std::cout << "> ";
}

int main() {
    HttpClient client;
    
    // Server checking with retries
    std::cout << "Checking server..." << std::endl;
    for (int i = 0; i < 3; i++) {
        if (client.isServerAvailable()) {
            std::cout << "✅ Server is !" << std::endl;
            break;
        }
        std::cout << "❌ try " << i+1 << "/3..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    showMenu();
    
    std::string cmd;
    while (std::getline(std::cin, cmd)) {
        if (cmd == "quit") break;
        else if (cmd == "help") showMenu();
        else if (cmd == "check") {
            if (client.isServerAvailable())
                std::cout << "✅ Server is running!" << std::endl;
            else
                std::cout << "❌ Server is not responding!" << std::endl;
        }
        else if (cmd == "ping") {
            std::cout << "→ Response: " << client.ping() << std::endl;
        }
        else if (cmd == "kiki") {
            std::cout << "→ Response: " << client.kiki() << std::endl;
        }
        else {
            std::cout << "❌ Unknown command. Type 'help' for help." << std::endl;
        }
        std::cout << "> ";
    }
    
    return 0;
}