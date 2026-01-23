#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

struct User {
    std::string username;
    std::string password; // Étape 1: plaintext (à remplacer par hash ensuite)
};

class Storage {
public:
    bool registerUser(const std::string& username, const std::string& password);
    bool validateLogin(const std::string& username, const std::string& password) const;

private:
    mutable std::mutex m_;
    std::unordered_map<std::string, User> users_;
};
