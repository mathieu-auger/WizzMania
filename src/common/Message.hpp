#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <chrono>
#include <vector>

enum class MessageType {
    TEXT,       
    LOGIN,     
    LOGOUT,    
    REGISTER,   
    COMMAND,    // (ping, kiki, etc)
    ERROR,      
    WIZZ        
};

class Message {
private:
    MessageType type;
    std::string sender;
    std::string content;
    std::string receiver;      
    std::chrono::system_clock::time_point timestamp;
    int id;                    
    
public:
    // Constructors
    Message();
    Message(MessageType type, const std::string& sender, const std::string& content);
    Message(MessageType type, const std::string& sender, const std::string& content, const std::string& receiver);
    
    // Getters
    MessageType getType() const;
    std::string getTypeString() const;
    std::string getSender() const;
    std::string getContent() const;
    std::string getReceiver() const;
    std::string getTimestamp() const;
    int getId() const;
    
    // Setters
    void setType(MessageType type);
    void setSender(const std::string& sender);
    void setContent(const std::string& content);
    void setReceiver(const std::string& receiver);
    
    // Utility methods
    bool isValid() const;
    std::string toString() const;
    
    // Static factory methods for common message types
    static Message createLogin(const std::string& username);
    static Message createText(const std::string& sender, const std::string& text);
    static Message createCommand(const std::string& cmd);
    static Message createWizz(const std::string& from);
};

#endif