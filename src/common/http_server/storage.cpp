#include "storage.hpp"

bool Storage::registerUser(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) return false;

    std::scoped_lock lock(m_);
    if (users_.find(username) != users_.end()) return false;

    users_.emplace(username, User{username, password});
    return true;
}

bool Storage::validateLogin(const std::string& username, const std::string& password) const {
    std::scoped_lock lock(m_);
    auto it = users_.find(username);
    if (it == users_.end()) return false;
    return it->second.password == password;
}
