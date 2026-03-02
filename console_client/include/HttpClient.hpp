#ifndef HTTPCLIENT_HPP
#define HTTPCLIENT_HPP

#include <string>
#include <vector>
#include "../../src/common/Protocol.hpp" 

#include "../../src/common/Message.hpp"

class HttpClient {
private:
    std::string serverHost;
    int serverPort;

    std::string urlEncode(const std::string& str);
    
public:
    HttpClient(const std::string& host = "127.0.0.1", int port = 18080);
    ~HttpClient();
    
    // Http methods basics
    std::string get(const std::string& path);
    std::string post(const std::string& path, const std::string& data = "");
    
    // Specific methods for application
    std::string ping();
    std::string chat();
    bool isServerAvailable();

    std::string registerUser(const std::string& username, const std::string& password);
    std::string login(const std::string& username, const std::string& password);
    std::string sendMessage(const std::string& message, const std::string& from);
    std::string getMessages();

    std::vector<Message> getMessagesAsObjects();
    bool sendMessage(const Message& msg);
};

#endif