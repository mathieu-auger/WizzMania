#include "database.hpp"
#include <iostream>


static bool exec_sql(sqlite3* db, const char* sql)
{
    char* err = nullptr;

    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::cerr << "[SQL ERROR] "
                  << (err ? err : "unknown")
                  << std::endl;

        sqlite3_free(err);
        return false;
    }

    return true;
}


Database::Database(const std::string& path)
    : db_(nullptr)
{
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
    {
        std::cerr << "Failed to open database: "
                  << sqlite3_errmsg(db_)
                  << std::endl;

        db_ = nullptr;
    }
}


Database::~Database()
{
    if (db_)
        sqlite3_close(db_);
}


bool Database::init()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!db_)
        return false;

    sqlite3_busy_timeout(db_, 2000);

    exec_sql(db_, "PRAGMA journal_mode=WAL;");
    exec_sql(db_, "PRAGMA synchronous=NORMAL;");
    exec_sql(db_, "PRAGMA foreign_keys=ON;");

    const char* schema = R"SQL(
CREATE TABLE IF NOT EXISTS friend_requests (
  from_user  TEXT NOT NULL,
  to_user    TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  PRIMARY KEY(from_user, to_user)
);

CREATE TABLE IF NOT EXISTS friends (
  user_a     TEXT NOT NULL,
  user_b     TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  PRIMARY KEY(user_a, user_b)
);

CREATE TABLE IF NOT EXISTS messages (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  from_user    TEXT NOT NULL,
  to_user      TEXT NOT NULL,
  body         TEXT NOT NULL,
  created_at   INTEGER NOT NULL,
  delivered_at INTEGER,
  seen_at      INTEGER
);

CREATE INDEX IF NOT EXISTS idx_messages_to_delivered
ON messages(to_user, delivered_at);

CREATE INDEX IF NOT EXISTS idx_messages_pair_id
ON messages(from_user, to_user, id);

CREATE INDEX IF NOT EXISTS idx_messages_pair_id_rev
ON messages(to_user, from_user, id);
)SQL";

    return exec_sql(db_, schema);
}


bool Database::areFriends(const std::string& u1,
                          const std::string& u2)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!db_)
        return false;

    const char* sql =
        "SELECT 1 FROM friends WHERE user_a=? AND user_b=?;";

    sqlite3_stmt* st = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, u1.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, u2.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(st) == SQLITE_ROW);

    sqlite3_finalize(st);

    return ok;
}