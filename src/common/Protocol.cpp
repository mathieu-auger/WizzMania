#include "Protocol.hpp"
#include <sstream>
#include <iostream>

std::string Protocol::serialize(const Message& msg) {
    std::stringstream ss;
    
    // Format TYPE|SENDER|CONTENT|RECEIVER|TIMESTAMP|ID
    ss << msg.getTypeString() << DELIMITER
       << msg.getSender() << DELIMITER
       << msg.getContent() << DELIMITER
       << msg.getReceiver() << DELIMITER
       << msg.getTimestamp() << DELIMITER
       << msg.getId()
       << END_MARKER;
    
    return ss.str();
}

Message Protocol::deserialize(const std::string& data) {
    std::stringstream ss(data);
    std::string token;
    std::vector<std::string> tokens;
    
    // Split the string by delimiter
    while (std::getline(ss, token, DELIMITER[0])) {
        tokens.push_back(token);
    }
    
    if (tokens.size() < 6) {
        return Message(); 
    }
    
    // Determine message type
    MessageType type = MessageType::TEXT;
    if (tokens[0] == "LOGIN") type = MessageType::LOGIN;
    else if (tokens[0] == "LOGOUT") type = MessageType::LOGOUT;
    else if (tokens[0] == "REGISTER") type = MessageType::REGISTER;
    else if (tokens[0] == "COMMAND") type = MessageType::COMMAND;
    else if (tokens[0] == "ERROR") type = MessageType::ERROR;
    else if (tokens[0] == "WIZZ") type = MessageType::WIZZ;
    
    Message msg(type, tokens[1], tokens[2], tokens[3]);
    return msg;
}

bool Protocol::isValid(const std::string& data) {
    // Check if the data contains the required number of delimiters
    int delimiterCount = 0;
    for (char c : data) {
        if (c == DELIMITER[0]) delimiterCount++;
    }
    return delimiterCount >= 4; 
}

std::vector<std::string> Protocol::splitMessages(const std::string& buffer) {
    std::vector<std::string> messages;
    std::stringstream ss(buffer);
    std::string msg;
    
    while (std::getline(ss, msg, END_MARKER[0])) {
        if (!msg.empty()) {
            messages.push_back(msg);
        }
    }
    
    return messages;
}

std::string Protocol::toHttpBody(const Message& msg) {
    // Convert Message to a simple key=value format for HTTP body
    std::stringstream ss;
    
    switch(msg.getType()) {
        case MessageType::LOGIN:
        case MessageType::REGISTER:
            ss << "username=" << msg.getSender()
               << "&password=" << msg.getContent();
            break;
            
        case MessageType::TEXT:
            ss << "message=" << msg.getContent()
               << "&from=" << msg.getSender();
            if (!msg.getReceiver().empty()) {
                ss << "&to=" << msg.getReceiver();
            }
            break;
            
        case MessageType::COMMAND:
            ss << "cmd=" << msg.getContent();
            break;
            
        default:
            ss << "data=" << msg.getContent();
    }
    
    return ss.str();
}

Message Protocol::fromHttpResponse(const std::string& response) {
    // Extract the body from the HTTP response
    size_t bodyPos = response.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        std::string body = response.substr(bodyPos + 4);
        
        if (body.find("{") == 0) {
            // TODO: Parse JSON response to create Message
            return Message(MessageType::TEXT, "server", body);
        } else {
            return Message(MessageType::TEXT, "server", body);
        }
    }
    
    return Message(MessageType::ERROR, "system", "Invalid response");
}