#include "../include/HttpClient.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <chrono>

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

// ========== Constructor/Destructor ==========

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


std::string HttpClient::urlEncode(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            result += buf;
        }
    }
    return result;
}

// ========== HTTP methodes basics ==========

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

std::string HttpClient::post(const std::string& path, const std::string& data) {
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
    
    std::string request = "POST " + path + " HTTP/1.1\r\n"
                          "Host: " + serverHost + "\r\n"
                          "Content-Type: application/x-www-form-urlencoded\r\n"
                          "Content-Length: " + std::to_string(data.length()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" +
                          data;
    
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

// ========== API method ==========

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

std::string HttpClient::registerUser(const std::string& username, const std::string& password) {
    std::string data = "username=" + urlEncode(username) + "&password=" + urlEncode(password);
    return post("/register", data);
}

std::string HttpClient::login(const std::string& username, const std::string& password) {
    std::string data = "username=" + urlEncode(username) + "&password=" + urlEncode(password);
    return post("/login", data);
}

std::string HttpClient::sendMessage(const std::string& message, const std::string& from) {
    std::string data = "message=" + urlEncode(message) + "&from=" + urlEncode(from);
    return post("/send", data);
}

std::string HttpClient::getMessages() {
    return get("/messages");
}

// ========== New methods with PROTOCOL ==========

std::vector<Message> HttpClient::getMessagesAsObjects() {
    std::string response = get(Protocol::MESSAGES_PATH);
    std::vector<Message> messages;
    
    auto parts = Protocol::splitMessages(response);
    for (const auto& part : parts) {
        if (Protocol::isValid(part)) {
            messages.push_back(Protocol::deserialize(part));
        }
    }
    
    return messages;
}

bool HttpClient::sendMessage(const Message& msg) {
    std::string body = Protocol::toHttpBody(msg);
    std::string response = post(Protocol::SEND_PATH, body);
    
    return response.find("success") != std::string::npos;
}