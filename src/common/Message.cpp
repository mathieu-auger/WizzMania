#include "Message.hpp"
#include <iomanip>
#include <sstream>
#include <ctime>

// Static variable to generate unique IDs for messages
static int nextId = 1;

Message::Message() 
    : type(MessageType::TEXT)
    , sender("")
    , content("")
    , receiver("")
    , timestamp(std::chrono::system_clock::now())
    , id(nextId++) 
{}

Message::Message(MessageType type, const std::string& sender, const std::string& content)
    : type(type)
    , sender(sender)
    , content(content)
    , receiver("")
    , timestamp(std::chrono::system_clock::now())
    , id(nextId++) 
{}

Message::Message(MessageType type, const std::string& sender, const std::string& content, const std::string& receiver)
    : type(type)
    , sender(sender)
    , content(content)
    , receiver(receiver)
    , timestamp(std::chrono::system_clock::now())
    , id(nextId++) 
{}

MessageType Message::getType() const {
    return type;
}

std::string Message::getTypeString() const {
    switch(type) {
        case MessageType::TEXT: return "TEXT";
        case MessageType::LOGIN: return "LOGIN";
        case MessageType::LOGOUT: return "LOGOUT";
        case MessageType::REGISTER: return "REGISTER";
        case MessageType::COMMAND: return "COMMAND";
        case MessageType::ERROR: return "ERROR";
        case MessageType::WIZZ: return "WIZZ";
        default: return "UNKNOWN";
    }
}

std::string Message::getSender() const {
    return sender;
}

std::string Message::getContent() const {
    return content;
}

std::string Message::getReceiver() const {
    return receiver;
}

std::string Message::getTimestamp() const {
    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    return ss.str();
}

int Message::getId() const {
    return id;
}

void Message::setType(MessageType type) {
    this->type = type;
}

void Message::setSender(const std::string& sender) {
    this->sender = sender;
}

void Message::setContent(const std::string& content) {
    this->content = content;
}

void Message::setReceiver(const std::string& receiver) {
    this->receiver = receiver;
}

bool Message::isValid() const {
    return !sender.empty() && (!content.empty() || type == MessageType::COMMAND);
}

std::string Message::toString() const {
    std::stringstream ss;
    ss << "[" << getTimestamp() << "] ";
    
    if (!receiver.empty() && receiver != "all") {
        ss << sender << " -> " << receiver << ": ";
    } else {
        ss << sender << ": ";
    }
    
    if (type == MessageType::WIZZ) {
        ss << "*** WIZZ ***";
    } else {
        ss << content;
    }
    
    return ss.str();
}

Message Message::createLogin(const std::string& username) {
    return Message(MessageType::LOGIN, username, "login");
}

Message Message::createText(const std::string& sender, const std::string& text) {
    return Message(MessageType::TEXT, sender, text);
}

Message Message::createCommand(const std::string& cmd) {
    return Message(MessageType::COMMAND, "system", cmd);
}

Message Message::createWizz(const std::string& from) {
    return Message(MessageType::WIZZ, from, "WIZZ");
}