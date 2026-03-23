#pragma once

#include <string>
#include <mutex>
#include <sqlite3.h>

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    bool init();
    bool areFriends(const std::string& u1,
                    const std::string& u2);

    sqlite3* handle() const;

private:
    sqlite3* db_;
    std::mutex mtx_;
}; 