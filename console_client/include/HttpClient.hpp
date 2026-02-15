#ifndef HTTPCLIENT_HPP
#define HTTPCLIENT_HPP

#include <string>
#include "../../src/common/Protocol.hpp" 

class HttpClient {
private:
    std::string serverHost;
    int serverPort;
    
public:
    HttpClient(const std::string& host = "127.0.0.1", int port = 18080);
    ~HttpClient();
    
    // Http methods basics
    std::string get(const std::string& path);
    std::string post(const std::string& path, const std::string& data = "");
    
    // Specific methods for application
    std::string ping();
    std::string kiki();
    bool isServerAvailable();
};

#endif