#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include "Message.hpp"
#include <string>
#include <vector>

class Protocol {
public:
    // Serialization of Message to string
    static std::string serialize(const Message& msg);
    
    // Deserialization of string to Message
    static Message deserialize(const std::string& data);
    
    // Validation of message format
    static bool isValid(const std::string& data);
    
    // Splitting multiple messages in a single buffer
    static std::vector<std::string> splitMessages(const std::string& buffer);
    
    // Conversion of Message to HTTP body format
    static std::string toHttpBody(const Message& msg);
    
    // Parsing HTTP response to Message
    static Message fromHttpResponse(const std::string& response);
    
    // Constants for protocol
    static constexpr const char* DELIMITER = "|";
    static constexpr const char* END_MARKER = "\n";
    
    // HTTP endpoints
    static constexpr const char* PING_PATH = "/ping";
    static constexpr const char* KIKI_PATH = "/kiki";
    static constexpr const char* REGISTER_PATH = "/register";
    static constexpr const char* LOGIN_PATH = "/login";
    static constexpr const char* SEND_PATH = "/send";
    static constexpr const char* MESSAGES_PATH = "/messages";
};

#endif