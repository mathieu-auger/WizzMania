#include "crow_all.h"
#include "routes.hpp"
#include "database.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>
#include <sqlite3.h>
#include <ctime>
#include <mutex>
#include <vector>
#include <optional>
#include <openssl/sha.h>
#include <algorithm>


struct PendingWyzz
{
    std::string from_user;   // username interne de l'expéditeur
    std::int64_t created_at; // timestamp
};

std::unordered_map<std::string, std::vector<PendingWyzz>> g_pendingWyzz;
std::mutex g_pendingWyzzMutex;
// ===== mémoire =====
static std::unordered_map<std::string, std::string> g_sessions;
static std::mutex g_db_mtx;
static std::unordered_set<std::string> g_usernames;
static std::unordered_set<std::string> g_emails;
static std::unordered_map<std::string, std::string> g_presence; // username -> status
static std::unordered_map<std::string, std::vector<crow::websocket::connection *>> g_ws_by_user;
static std::unordered_map<crow::websocket::connection *, std::string> g_user_by_ws;
static std::mutex g_ws_mtx;
std::unordered_map<std::string, std::vector<crow::json::wvalue>> g_pending_events;

static void ws_send_to_user(const std::string &username,
                            const std::string &payload)
{
    std::lock_guard<std::mutex> lock(g_ws_mtx);

    auto it = g_ws_by_user.find(username);
    if (it == g_ws_by_user.end())
        return;

    for (auto *c : it->second)
    {
        if (c)
            c->send_text(payload);
    }
}

static std::string get_query_param(const crow::request &req, const std::string &key)
{
    auto v = req.url_params.get(key);
    return v ? std::string(v) : std::string{};
}

static void ws_add(const std::string &user, crow::websocket::connection &conn)
{
    std::lock_guard<std::mutex> lock(g_ws_mtx);
    g_ws_by_user[user].push_back(&conn);
    g_user_by_ws[&conn] = user;
}

static void ws_remove(crow::websocket::connection &conn)
{
    std::lock_guard<std::mutex> lock(g_ws_mtx);

    auto itU = g_user_by_ws.find(&conn);
    if (itU == g_user_by_ws.end())
        return;

    const std::string user = itU->second;
    g_user_by_ws.erase(itU);

    auto it = g_ws_by_user.find(user);
    if (it != g_ws_by_user.end())
    {
        auto &vec = it->second;

        vec.erase(
            std::remove(vec.begin(), vec.end(), &conn),
            vec.end());

        if (vec.empty())
            g_ws_by_user.erase(it);
    }

    std::cout << "[WS] disconnected: " << user << std::endl;
}

static const std::string USERS_PATH = "users.jsonl";
static sqlite3 *g_db = nullptr;
static const char *DB_PATH = "app.db";
static crow::response db_prepare_error(const char *where)
{
    const char *msg = (g_db ? sqlite3_errmsg(g_db) : "g_db is null");
    std::cout << "[DB] " << where << " prepare failed: " << msg << "\n";
    return crow::response(500, std::string("DB prepare failed: ") + msg);
}

static crow::response db_step_error(const char *where, int rc)
{
    const char *msg = (g_db ? sqlite3_errmsg(g_db) : "g_db is null");
    std::cout << "[DB] " << where << " step failed rc=" << rc << ": " << msg << "\n";
    return crow::response(500, std::string("DB step failed: ") + msg);
}

static bool init_db()
{
    int rc = sqlite3_open_v2(DB_PATH, &g_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);

    if (rc != SQLITE_OK)
    {
        std::cerr << "sqlite open failed:" << (g_db ? sqlite3_errmsg(g_db) : "null") << "/n";
        return false;
    }

    sqlite3_busy_timeout(g_db, 2000);
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    return true;
}

static bool db_exec(const char *sql)
{
    char *err = nullptr;

    int rc = sqlite3_exec(g_db, sql, nullptr, nullptr, &err);

    if (rc != SQLITE_OK)
    {
        std::cout << "[SQL ERROR] " << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        return false;
    }

    return true;
}

// ===== utils =====
static long long now_unix()
{
    return static_cast<long long>(std::time(nullptr));
}
// ===== utils =====

static std::string make_token()
{
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<unsigned long long> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

static std::string get_bearer_token(const crow::request &req)
{
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end())
        return {};

    const std::string &v = it->second;
    const std::string prefix = "Bearer ";
    if (v.rfind(prefix, 0) != 0)
        return {};
    return v.substr(prefix.size());
}

static bool require_auth(const crow::request &req, std::string &out_username)
{
    const std::string token = get_bearer_token(req);
    if (token.empty())
        return false;

    auto it = g_sessions.find(token);
    if (it == g_sessions.end())
        return false;

    out_username = it->second;
    return true;
}

static bool append_line(const std::string &path, const std::string &line)
{
    std::ofstream out(path, std::ios::app);
    if (!out.is_open())
        return false;
    out << line << "\n";
    return true;
}

// ===== hash + salt (POC sérieux) =====
static std::string random_salt_hex(size_t bytes = 16)
{
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);

    std::ostringstream oss;
    for (size_t i = 0; i < bytes; ++i)
    {
        int b = dist(rng);
        oss << std::hex << std::setw(2) << std::setfill('0') << (b & 0xff);
    }
    return oss.str();
}

static std::string sha256_hex(const std::string &data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(data.data()), data.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

static std::string hash_password(const std::string &password, const std::string &salt_hex)
{
    return sha256_hex(salt_hex + ":" + password);
}

// ===== fichier users.jsonl =====
static void load_users_from_file(const std::string &path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        std::cout << "[LOAD] No existing file: " << path << " (first run)\n";
        return;
    }

    std::string line;
    int loaded = 0;
    int skipped = 0;

    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        auto j = crow::json::load(line);
        if (!j)
        {
            skipped++;
            continue;
        }

        // sécurité: ne jamais indexer sans has()
        if (!j.has("username") || !j.has("email"))
        {
            skipped++;
            continue;
        }

        std::string username = std::string(j["username"].s());
        std::string email = std::string(j["email"].s());

        if (username.empty() || email.empty())
        {
            skipped++;
            continue;
        }

        g_usernames.insert(username);
        g_emails.insert(email);
        loaded++;
    }

    std::cout << "[LOAD] users loaded=" << loaded << " skipped=" << skipped << " from " << path << "\n";
}

static bool check_credentials_in_file(const std::string &path,
                                      const std::string &username_or_email,
                                      const std::string &password_input,
                                      std::string &out_username)
{
    std::ifstream in(path);
    std::cout << "[LOGIN] open '" << path << "' -> " << (in.is_open() ? "OK" : "FAIL") << "\n";
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        auto u = crow::json::load(line);
        if (!u)
            continue;

        if (!u.has("username") || !u.has("email"))
            continue;

        std::string username = std::string(u["username"].s());
        std::string email = std::string(u["email"].s());

        const bool id_match = (username_or_email == username) || (username_or_email == email);
        if (!id_match)
            continue;

        // --- mode hash ---
        std::string salt, stored_hash;
        if (u.has("salt"))
            salt = std::string(u["salt"].s());
        if (u.has("password_hash"))
            stored_hash = std::string(u["password_hash"].s());

        if (!salt.empty() && !stored_hash.empty())
        {
            const std::string input_hash = hash_password(password_input, salt);
            if (input_hash == stored_hash)
            {
                out_username = username;
                return true;
            }
            return false;
        }

        // --- fallback mode dev ---
        std::string stored_pwd;
        if (u.has("password"))
            stored_pwd = std::string(u["password"].s());

        if (!stored_pwd.empty())
        {
            if (stored_pwd == password_input)
            {
                out_username = username;
                return true;
            }
            return false;
        }

        return false;
    }

    return false;
}

// ====SQLite ====
static void normalize_pair(std::string &a, std::string &b)
{
    if (b < a)
        std::swap(a, b);
}

static bool db_init()
{
    std::cout << "[DB] g_db pointer = " << g_db << std::endl;

    std::string db_path = (std::filesystem::current_path() / "app.db").string();
    std::cout << "[DB] opening " << db_path << std::endl;

    if (sqlite3_open(db_path.c_str(), &g_db) != SQLITE_OK)
    {
        std::cout << "[DB] cannot open " << db_path
                  << " : " << sqlite3_errmsg(g_db) << "\n";
        return false;
    }

    db_exec("PRAGMA journal_mode=WAL;");
    db_exec("PRAGMA synchronous=NORMAL;");

    const char *schema = R"SQL(

    PRAGMA foreign_keys = ON;

    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT NOT NULL UNIQUE,
        email TEXT NOT NULL UNIQUE,
        salt TEXT NOT NULL,
        password_hash TEXT NOT NULL,
        created_at INTEGER NOT NULL
    );

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

    CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
    CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);

    CREATE INDEX IF NOT EXISTS idx_messages_to_delivered
    ON messages(to_user, delivered_at);

    CREATE INDEX IF NOT EXISTS idx_messages_pair_id
    ON messages(from_user, to_user, id);

    CREATE INDEX IF NOT EXISTS idx_messages_pair_id_rev
    ON messages(to_user, from_user, id);

    )SQL";

    if (!db_exec(schema))
        return false;

    db_exec("ALTER TABLE users ADD COLUMN display_name TEXT;");

    std::cout << "[DB] ready: " << db_path << "\n";
    return true;
}

static void db_close()
{
    if (g_db)
    {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

static bool db_user_exists_username(const std::string &username)
{
    const char *sql = "SELECT 1 FROM users WHERE username=? LIMIT 1;";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return ok;
}

static bool db_user_exists_email(const std::string &email)
{
    const char *sql = "SELECT 1 FROM users WHERE email=? LIMIT 1;";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return ok;
}

static bool db_insert_user(const std::string &username,
                           const std::string &email,
                           const std::string &salt,
                           const std::string &password_hash,
                           long long created_at)
{
    const char *sql =
        "INSERT INTO users(username,email,salt,password_hash,created_at) "
        "VALUES(?,?,?,?,?);";

    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, password_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, (sqlite3_int64)created_at);

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    return rc == SQLITE_DONE;
}

struct DbUserAuth
{
    std::string username;
    std::string salt;
    std::string password_hash;
};

static std::optional<DbUserAuth> db_get_user_auth_by_id(const std::string &username_or_email)
{
    const char *sql =
        "SELECT username, salt, password_hash "
        "FROM users "
        "WHERE username=? OR email=? "
        "LIMIT 1;";

    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(st, 1, username_or_email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, username_or_email.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(st);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(st);
        return std::nullopt;
    }

    DbUserAuth u;
    const unsigned char *c0 = sqlite3_column_text(st, 0);
    const unsigned char *c1 = sqlite3_column_text(st, 1);
    const unsigned char *c2 = sqlite3_column_text(st, 2);

    u.username = c0 ? reinterpret_cast<const char *>(c0) : "";
    u.salt = c1 ? reinterpret_cast<const char *>(c1) : "";
    u.password_hash = c2 ? reinterpret_cast<const char *>(c2) : "";

    sqlite3_finalize(st);

    if (u.username.empty() || u.salt.empty() || u.password_hash.empty())
        return std::nullopt;

    return u;
}

bool db_are_friends(const std::string& u1, const std::string& u2)
{
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT 1 FROM friends "
        "WHERE (user_a=? AND user_b=?) "
        "   OR (user_a=? AND user_b=?) "
        "LIMIT 1;";

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, u1.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, u2.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, u2.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, u1.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return ok;
}

static bool db_request_exists(const std::string &from, const std::string &to)
{
    const char *sql = "SELECT 1 FROM friend_requests WHERE from_user=? AND to_user=?;";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(st, 1, from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, to.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return ok;
}

// ===== ROUTES =====
void register_routes(crow::SimpleApp &app, Database &db)
{
    // fichier dans build/server/
    if (!db_init())
    {
        std::cout << "[DB] init failed\n";
    }
    load_users_from_file(USERS_PATH);

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onaccept([&](const crow::request &req,
                      std::optional<crow::response> &res,
                      void **userdata)
                  {

    const std::string token = get_query_param(req, "token");
    if (token.empty()) {
        res = crow::response(401, "Missing token");
        return;
    }

    auto it = g_sessions.find(token);
    if (it == g_sessions.end()) {
        res = crow::response(401, "Invalid token");
        return;
    }

    // OK: on accepte et on stocke le username dans userdata
    *userdata = new std::string(it->second); })
        .onopen([&](crow::websocket::connection &conn)
                {
    auto* u = static_cast<std::string*>(conn.userdata());
    if (!u) return;

    const std::string& username = *u;

    std::lock_guard<std::mutex> lock(g_ws_mtx);
    g_ws_by_user[username].push_back(&conn);
    g_user_by_ws[&conn] = username;

    std::cout << "[WS] open user=" << username << "\n"; })
        .onmessage([&](crow::websocket::connection &conn, const std::string &data, bool is_binary)
                   {
    (void)is_binary;

    auto* u = static_cast<std::string*>(conn.userdata());
    if (!u) return;

    const std::string& from = *u;
    std::cout << "[WS] msg from=" << from << " data=" << data << "\n"; })
        .onclose([&](crow::websocket::connection &conn, const std::string &reason, uint16_t code)
                 {
    (void)reason;
    (void)code;

    auto* u = static_cast<std::string*>(conn.userdata());
    if (!u) return;

    const std::string username = *u;

    {
        std::lock_guard<std::mutex> lock(g_ws_mtx);

        auto it = g_ws_by_user.find(username);
        if (it != g_ws_by_user.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), &conn), vec.end());
            if (vec.empty()) g_ws_by_user.erase(it);
        }

        g_user_by_ws.erase(&conn);
    }

    delete u;
    conn.userdata(nullptr);

    std::cout << "[WS] close user=" << username << "\n"; });

    CROW_ROUTE(app, "/dm/send").methods(crow::HTTPMethod::Post)
([](const crow::request& req)
{
    std::string from;
    if (!require_auth(req, from))
        return crow::response(401, "Unauthorized");

    if (!db_init())
        return crow::response(500, "DB not ready");

    auto j = crow::json::load(req.body);
    if (!j)
        return crow::response(400, "Invalid JSON");

    if (!j.has("to") || !j.has("text"))
        return crow::response(400, "Missing field");

    const std::string to_display = std::string(j["to"].s());
    const std::string text = std::string(j["text"].s());

    if (to_display.empty() || text.empty())
        return crow::response(400, "Empty fields");

    // On interdit l'auto-envoi par pseudo ou par id
    if (to_display == from)
        return crow::response(400, "You cannot send a message to yourself");

    // Traduction pseudo (display_name) -> username interne (id sous forme de texte)
    std::string to;
    {
        const char* sql =
            "SELECT username FROM users WHERE display_name = ? LIMIT 1;";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
            return crow::response(500, std::string("DB prepare failed: ") + sqlite3_errmsg(g_db));

        sqlite3_bind_text(st, 1, to_display.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(st) == SQLITE_ROW) {
            const char* to_c = reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            if (to_c)
                to = to_c;
        }

        sqlite3_finalize(st);
    }

    if (to.empty())
        return crow::response(404, "User not found");

    if (to == from)
        return crow::response(400, "You cannot send a message to yourself");

    // Vérifie l'amitié avec l'id interne
    if (!db_are_friends(from, to))
        return crow::response(403, "Not friends");

    const char* sql =
        "INSERT INTO messages(from_user, to_user, body, created_at) "
        "VALUES(?,?,?,?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return crow::response(500, std::string("DB prepare failed: ") + sqlite3_errmsg(g_db));

    sqlite3_bind_text(st, 1, from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, to.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)now_unix());

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE)
        return crow::response(500, std::string("DB insert failed: ") + sqlite3_errmsg(g_db));

    crow::json::wvalue res;
    res["ok"] = true;
    res["from"] = from;          // id interne
    res["to"] = to;              // id interne
    res["to_display"] = to_display; // pseudo utilisé côté UI
    res["message"] = "sent";

    return crow::response(200, res);
});

    CROW_ROUTE(app, "/dm/history").methods(crow::HTTPMethod::Get)
([&db](const crow::request& req)
{
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    const char* other_c = req.url_params.get("with");
    if (!other_c)
        return crow::response(400, "Missing 'with'");

    std::string other_display = other_c;
    if (other_display.empty())
        return crow::response(400, "Empty 'with'");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    // Conversion pseudo (display_name) -> username interne (id texte)
    std::string other;
    {
        const char* sql =
            "SELECT username FROM users WHERE display_name = ? LIMIT 1;";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(conn, sql, -1, &st, nullptr) != SQLITE_OK)
            return crow::response(500, std::string("DB prepare failed (user lookup): ") + sqlite3_errmsg(conn));

        sqlite3_bind_text(st, 1, other_display.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(st) == SQLITE_ROW) {
            const char* other_id_c =
                reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            if (other_id_c)
                other = other_id_c;
        }

        sqlite3_finalize(st);
    }

    if (other.empty())
        return crow::response(404, "User not found");

    if (!db.areFriends(me, other))
        return crow::response(403, "Not friends");

    // Récupérer le display_name de "me" pour garder une structure homogène
    std::string me_display;
    {
        const char* sql =
            "SELECT display_name FROM users WHERE username = ? LIMIT 1;";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(conn, sql, -1, &st, nullptr) != SQLITE_OK)
            return crow::response(500, std::string("DB prepare failed (me lookup): ") + sqlite3_errmsg(conn));

        sqlite3_bind_text(st, 1, me.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(st) == SQLITE_ROW) {
            const char* me_display_c =
                reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            if (me_display_c)
                me_display = me_display_c;
        }

        sqlite3_finalize(st);
    }

    sqlite3_stmt* stmt = nullptr;

    const char* sql = R"SQL(
SELECT
    m.id,
    m.from_user,
    m.to_user,
    m.body,
    m.created_at,
    m.delivered_at,
    m.seen_at,
    uf.display_name AS from_name,
    ut.display_name AS to_name
FROM messages m
LEFT JOIN users uf ON m.from_user = uf.username
LEFT JOIN users ut ON m.to_user = ut.username
WHERE
    (m.from_user = ? AND m.to_user = ?)
    OR
    (m.from_user = ? AND m.to_user = ?)
ORDER BY m.id DESC
LIMIT 50;
)SQL";

    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return crow::response(500, std::string("DB prepare failed: ") + sqlite3_errmsg(conn));
    }

    sqlite3_bind_text(stmt, 1, me.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, other.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, other.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, me.c_str(),    -1, SQLITE_TRANSIENT);

    crow::json::wvalue out = crow::json::wvalue::list();
    int i = 0;

    // ===== messages normaux =====
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        crow::json::wvalue m;

        const char* from_c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* to_c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* body_c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* from_name_c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* to_name_c = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));

        std::string from = from_c ? from_c : "";
        std::string to = to_c ? to_c : "";
        std::string body = body_c ? body_c : "";
        std::string from_name = from_name_c ? from_name_c : "";
        std::string to_name = to_name_c ? to_name_c : "";

        m["type"] = "message";          // <-- ajout
        m["id"] = sqlite3_column_int64(stmt, 0);
        m["from"] = from;               // id interne
        m["to"] = to;                   // id interne
        m["body"] = body;
        m["created_at"] = sqlite3_column_int64(stmt, 4);
        m["from_name"] = from_name;     // pseudo affiché
        m["to_name"] = to_name;         // pseudo affiché

        if (sqlite3_column_type(stmt, 5) == SQLITE_NULL)
            m["delivered_at"] = nullptr;
        else
            m["delivered_at"] = sqlite3_column_int64(stmt, 5);

        if (sqlite3_column_type(stmt, 6) == SQLITE_NULL)
            m["seen_at"] = nullptr;
        else
            m["seen_at"] = sqlite3_column_int64(stmt, 6);

        out[i++] = std::move(m);
    }

    sqlite3_finalize(stmt);

    // ===== wyzz en attente pour CETTE conversation uniquement =====
    std::vector<PendingWyzz> wyzzForThisChat;

    {
        std::lock_guard<std::mutex> lock(g_pendingWyzzMutex);

        auto it = g_pendingWyzz.find(me);
        if (it != g_pendingWyzz.end())
        {
            auto& vec = it->second;

            auto splitIt = std::stable_partition(
                vec.begin(),
                vec.end(),
                [&](const PendingWyzz& w)
                {
                    return w.from_user != other;
                }
            );

            for (auto p = splitIt; p != vec.end(); ++p)
                wyzzForThisChat.push_back(*p);

            vec.erase(splitIt, vec.end());

            if (vec.empty())
                g_pendingWyzz.erase(it);
        }
    }

    // On les ajoute dans la réponse
    for (const auto& w : wyzzForThisChat)
    {
        crow::json::wvalue evt;
        evt["type"] = "wyzz";
        evt["id"] = 0;
        evt["from"] = w.from_user;           // username interne
        evt["to"] = me;                      // username interne
        evt["body"] = "*** WYZZ ***";
        evt["created_at"] = w.created_at;
        evt["from_name"] = other_display;    // pseudo affiché de l'autre côté du chat
        evt["to_name"] = me_display;

        evt["delivered_at"] = nullptr;
        evt["seen_at"] = nullptr;

        out[i++] = std::move(evt);
    }

    return crow::response(200, out);
});

CROW_ROUTE(app, "/account/display-name").methods(crow::HTTPMethod::Post)
([&db](const crow::request& req)
{
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    auto j = crow::json::load(req.body);
    if (!j || !j.has("display_name"))
        return crow::response(400, "Invalid JSON");

    std::string newDisplay = std::string(j["display_name"].s());
    if (newDisplay.empty())
        return crow::response(400, "Empty display_name");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    // Vérifier unicité (important)
    {
        const char* checkSql =
            "SELECT 1 FROM users WHERE display_name = ? LIMIT 1;";

        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(conn, checkSql, -1, &st, nullptr);
        sqlite3_bind_text(st, 1, newDisplay.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(st) == SQLITE_ROW)
        {
            sqlite3_finalize(st);
            return crow::response(409, "Display name already taken");
        }

        sqlite3_finalize(st);
    }

    const char* sql =
        "UPDATE users SET display_name = ? WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return crow::response(500, sqlite3_errmsg(conn));

    sqlite3_bind_text(stmt, 1, newDisplay.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::string err = sqlite3_errmsg(conn);
        sqlite3_finalize(stmt);
        return crow::response(500, err);
    }

    sqlite3_finalize(stmt);

    std::cout << "[ACCOUNT] display_name updated for " << me << " -> " << newDisplay << "\n";

    return crow::response(200, "Display name updated");
});

CROW_ROUTE(app, "/account/email").methods(crow::HTTPMethod::Post)
([&db](const crow::request& req)
{
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    auto j = crow::json::load(req.body);
    if (!j || !j.has("email"))
        return crow::response(400, "Invalid JSON");

    std::string newEmail = std::string(j["email"].s());
    if (newEmail.empty())
        return crow::response(400, "Empty email");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    // Vérifier unicité
    {
        const char* checkSql =
            "SELECT 1 FROM users WHERE email = ? LIMIT 1;";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(conn, checkSql, -1, &st, nullptr) != SQLITE_OK)
            return crow::response(500, std::string("DB prepare failed (email check): ") + sqlite3_errmsg(conn));

        sqlite3_bind_text(st, 1, newEmail.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(st) == SQLITE_ROW)
        {
            sqlite3_finalize(st);
            return crow::response(409, "Email already taken");
        }

        sqlite3_finalize(st);
    }

    const char* sql =
        "UPDATE users SET email = ? WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return crow::response(500, std::string("DB prepare failed (email update): ") + sqlite3_errmsg(conn));

    sqlite3_bind_text(stmt, 1, newEmail.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::string err = sqlite3_errmsg(conn);
        sqlite3_finalize(stmt);
        return crow::response(500, err);
    }

    sqlite3_finalize(stmt);

    std::cout << "[ACCOUNT] email updated for " << me << " -> " << newEmail << "\n";
    return crow::response(200, "Email updated");
});

CROW_ROUTE(app, "/account/birthdate").methods(crow::HTTPMethod::Post)
([&db](const crow::request& req)
{
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    auto j = crow::json::load(req.body);
    if (!j || !j.has("birthdate"))
        return crow::response(400, "Invalid JSON");

    std::string newBirthdate = std::string(j["birthdate"].s());
    if (newBirthdate.empty())
        return crow::response(400, "Empty birthdate");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    const char* sql =
        "UPDATE users SET birthdate = ? WHERE username = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return crow::response(500, std::string("DB prepare failed (birthdate update): ") + sqlite3_errmsg(conn));

    sqlite3_bind_text(stmt, 1, newBirthdate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        std::string err = sqlite3_errmsg(conn);
        sqlite3_finalize(stmt);
        return crow::response(500, err);
    }

    sqlite3_finalize(stmt);

    std::cout << "[ACCOUNT] birthdate updated for " << me << " -> " << newBirthdate << "\n";
    return crow::response(200, "Birthdate updated");
});

CROW_ROUTE(app, "/account/delete").methods(crow::HTTPMethod::Post)
([&db](const crow::request& req)
{
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    char* errMsg = nullptr;

    if (sqlite3_exec(conn, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        std::string err = errMsg ? errMsg : "BEGIN failed";
        sqlite3_free(errMsg);
        return crow::response(500, err);
    }

    auto rollbackAndFail = [&](const std::string& errText)
    {
        char* rollbackErr = nullptr;
        sqlite3_exec(conn, "ROLLBACK;", nullptr, nullptr, &rollbackErr);
        if (rollbackErr)
            sqlite3_free(rollbackErr);
        return crow::response(500, errText);
    };

    auto execPrepared = [&](const char* sql) -> bool
    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return false;

        sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, me.c_str(), -1, SQLITE_TRANSIENT);

        const int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    };

    // Messages
    {
        const char* sql = "DELETE FROM messages WHERE from_user = ? OR to_user = ?;";
        if (!execPrepared(sql))
            return rollbackAndFail(std::string("Delete messages failed: ") + sqlite3_errmsg(conn));
    }

    // Amis
    {
        const char* sql = "DELETE FROM friends WHERE user_a = ? OR user_b = ?;";
        if (!execPrepared(sql))
            return rollbackAndFail(std::string("Delete friends failed: ") + sqlite3_errmsg(conn));
    }

    // Demandes d'amis
    {
        const char* sql = "DELETE FROM friend_requests WHERE from_user = ? OR to_user = ?;";
        if (!execPrepared(sql))
            return rollbackAndFail(std::string("Delete friend requests failed: ") + sqlite3_errmsg(conn));
    }

    // Utilisateur
    {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "DELETE FROM users WHERE username = ?;";
        if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) != SQLITE_OK)
            return rollbackAndFail(std::string("Prepare user delete failed: ") + sqlite3_errmsg(conn));

        sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            return rollbackAndFail(std::string("Delete user failed: ") + sqlite3_errmsg(conn));
        }

        sqlite3_finalize(stmt);
    }

    if (sqlite3_exec(conn, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        std::string err = errMsg ? errMsg : "COMMIT failed";
        sqlite3_free(errMsg);
        return crow::response(500, err);
    }

    // Nettoyage mémoire des wyzz
    {
        std::lock_guard<std::mutex> lock(g_pendingWyzzMutex);

        g_pendingWyzz.erase(me);

        for (auto it = g_pendingWyzz.begin(); it != g_pendingWyzz.end(); ++it)
        {
            auto& vec = it->second;
            vec.erase(
                std::remove_if(
                    vec.begin(),
                    vec.end(),
                    [&](const PendingWyzz& w)
                    {
                        return w.from_user == me;
                    }
                ),
                vec.end()
            );
        }
    }

    std::cout << "[ACCOUNT] deleted user " << me << "\n";
    return crow::response(200, "Account deleted");
});

CROW_ROUTE(app, "/wyzz").methods(crow::HTTPMethod::Post)
([&db](const crow::request& req)
{
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    auto j = crow::json::load(req.body);
    if (!j || !j.has("to"))
        return crow::response(400, "Invalid JSON");

    std::string other_display = std::string(j["to"].s());
    if (other_display.empty())
        return crow::response(400, "Empty target");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    // Conversion pseudo affiché -> username interne
    std::string other;
    {
        const char* sql =
            "SELECT username FROM users WHERE display_name = ? LIMIT 1;";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(conn, sql, -1, &st, nullptr) != SQLITE_OK)
            return crow::response(
                500,
                std::string("DB prepare failed (user lookup): ") + sqlite3_errmsg(conn)
            );

        sqlite3_bind_text(st, 1, other_display.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(st) == SQLITE_ROW)
        {
            const char* other_id_c =
                reinterpret_cast<const char*>(sqlite3_column_text(st, 0));
            if (other_id_c)
                other = other_id_c;
        }

        sqlite3_finalize(st);
    }

    if (other.empty())
        return crow::response(404, "User not found");

    if (!db.areFriends(me, other))
        return crow::response(403, "Not friends");

    // Stockage du wyzz en attente pour le receveur
    {
        std::lock_guard<std::mutex> lock(g_pendingWyzzMutex);
        g_pendingWyzz[other].push_back({ me, std::time(nullptr) });
    }

    return crow::response(200, "Wyzz stored");
});

    CROW_ROUTE(app, "/friends").methods(crow::HTTPMethod::Get)([](const crow::request &req)
                                                               {
        std::string me;
        if(!require_auth(req, me))
        return crow::response(401, "Unauthorized");

        crow::json::wvalue res;
        res["ok"] = true;
        res["me"] = me;
        res["friends"] = crow::json::wvalue::list();
        res["incoming"] = crow::json::wvalue::list();
        res["outgoing"] = crow::json::wvalue::list();
        res["presence"] = crow::json::wvalue::object();


    {
        const char* sql =
        "SELECT user_a, user_b FROM friends WHERE user_a = ? OR user_b = ?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st,1, me.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, me.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(st) == SQLITE_ROW) {
                std::string a = (const char*)sqlite3_column_text(st, 0);
                std::string b = (const char*)sqlite3_column_text(st, 1);
                std::string other = (a == me) ?b : a;

                res["friends"][res["friends"].size()] = other;

                auto it = g_presence.find(other);
                res["presence"][other] = (it != g_presence.end()) ? it ->second : "offline";
            }
        }
        sqlite3_finalize(st);
    }

    // incoming

    {
        const char* sql = " SELECT to_user FROM friends_requests WHERE from_users=?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, me.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW){
                res["incoming"][res["incoming"].size()] =
                std::string((const char*)sqlite3_column_text(st, 0));
            }
        }
        sqlite3_finalize(st);
    }

    // outgoing
    {
        const char* sql = "SELECT to_user FROM friend_request WHERE from_user=?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) == SQLITE_OK){
            sqlite3_bind_text(st, 1, me.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                res["outgoing"][res["outgoing"].size()] =
                std::string((const char*)sqlite3_column_text(st, 0));
            }
        }
        sqlite3_finalize(st);
    }
    return crow::response(200, res); });

    CROW_ROUTE(app, "/friends/request").methods(crow::HTTPMethod::Post)
([](const crow::request &req)
{
    // auth
    std::string me;
    if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

    // db
    if (!db_init())
        return crow::response(500, "DB not ready");

    // parse json
    auto j = crow::json::load(req.body);
    if (!j || !j.has("to"))
        return crow::response(400, "Missing field: to");

    std::string to = std::string(j["to"].s());
    if (to.empty())
        return crow::response(400, "Empty 'to'");

    if (to == me)
        return crow::response(400, "Cannot add yourself");

    // user must exist
    {
        const char* sql = "SELECT 1 FROM users WHERE username = ? LIMIT 1;";
        sqlite3_stmt* st = nullptr;

        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
            return db_prepare_error("friends/request user check");

        sqlite3_bind_text(st, 1, to.c_str(), -1, SQLITE_TRANSIENT);

        bool exists = (sqlite3_step(st) == SQLITE_ROW);
        sqlite3_finalize(st);

        if (!exists)
            return crow::response(404, "User not found");
    }

    // already friends?
    if (db_are_friends(me, to))
        return crow::response(409, "Already friends");

    // already sent ?
    if (db_request_exists(me, to))
        return crow::response(409, "Request already sent");

    // if reverse request exists
    if (db_request_exists(to, me))
        return crow::response(409, "Incoming request exist. Use /friends/accept");

    // insert request
    const char *sql =
        "INSERT OR IGNORE INTO friend_requests(from_user,to_user,created_at) VALUES(?,?,?);";

    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return db_prepare_error("friends/request insert");

    std::cout << me.c_str() << std::endl;
    std::cout << to.c_str() << std::endl;

    sqlite3_bind_text(st, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, to.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)now_unix());

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE)
        return db_step_error("friends/request insert", rc);

    // response Json
    crow::json::wvalue res;
    res["ok"] = true;
    res["from"] = me;
    res["to"] = to;
    res["message"] = "request sent";
    return crow::response(200, res);
});

    CROW_ROUTE(app, "/friends/accept").methods(crow::HTTPMethod::Post)([](const crow::request &req)
                                                                       {
        std::string me;
        if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

        if (!db_init())
        return crow::response(500, "DB not ready");

        auto j = crow::json::load(req.body);
        if (!j || !j.has("from"))
        return crow::response(400, "Missing field: from");

        std::string from = std::string(j["from"].s());
        if (from.empty())
        return crow::response(400, "Empty 'from'");

        if (from == me)
        return crow::response(400, "Invalid 'from'");

        //vérifie que la request existe (from -> me)

        {
            const char* sql =
            "SELECT 1 FROM friend_requests WHERE from_user=? AND to_user=?;";

            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
            return db_prepare_error("accept: select request");


            sqlite3_bind_text(st, 1, from.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, me.c_str(), -1, SQLITE_TRANSIENT);

            bool exists = (sqlite3_step(st) == SQLITE_ROW);
            sqlite3_finalize(st);

            if (!exists)
            return crow::response(404, " No incoming request");
        }


        {
            const char* sql =
            "DELETE FROM friend_requests WHERE from_user=? AND to_user=?;";

            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
            return db_prepare_error("accept: delete request");

            sqlite3_bind_text(st, 1, from.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(st, 2, me.c_str(), -1, SQLITE_TRANSIENT);

            sqlite3_step(st);
            sqlite3_finalize(st);
        }

        {
        std::string a = me, b =from;
        normalize_pair(a, b);

        const char* sql = "INSERT OR IGNORE INTO friends(user_a,user_b,created_at) VALUES(?,?,?);";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return db_prepare_error("accept: insert friend");

        sqlite3_bind_text(st, 1, a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, b.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 3, (sqlite3_int64)now_unix());

        int rc = sqlite3_step(st);
        sqlite3_finalize(st);

        if(rc != SQLITE_DONE)
        return crow::response(500,"DB insert failed");
        }

        crow::json::wvalue res;
        res["ok"] = true;
        res["message"] = "accepted";
        res["friend"] = from;
        return crow::response(200, res); });

    CROW_ROUTE(app, "/friend").methods(crow::HTTPMethod::Get)([](const crow::request &req)
                                                              {

        std::string me;
        if (!require_auth(req, me))
        return crow::response(401, "Unautorized");

        if (!db_init())
        return crow::response(500, "DB not ready");

        const char* sql =
        "SELECT user_a, user_b, created_at "
        "FROM friends "
        "WHERE user_a = ? OR user_b = ? "
        "ORDER BY created_at DESC;";

        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK) {
            return crow::response(500, std::string("DB prepare failed: ") + sqlite3_errmsg(g_db));

        }

        sqlite3_bind_text(st, 1, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, me.c_str(), -1, SQLITE_TRANSIENT);

        crow::json::wvalue res;
        res["OK"] = true;
        res["friends"] = crow::json::wvalue::list();

        int i = 0;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* a_c = (const char*)sqlite3_column_text(st, 0);
            const char* b_c = (const char*)sqlite3_column_text(st, 1);

            std::string a = a_c ? a_c : "";
            std::string b = b_c ? b_c : "";

            std::string other = (a == me) ? b: a;

            std::string status = "offline";
            auto it = g_presence.find(other);
            if (it != g_presence.end() && !it->second.empty())
            status = it->second;

            res["friends"][i]["username"] = other;
            res["friends"][i]["status"] = status;
            i++;
        }

        sqlite3_finalize(st);
        return crow::response(200, res); });

    CROW_ROUTE(app, "/ping").methods(crow::HTTPMethod::Get)([]
                                                            { return crow::response(200, "pong"); });

    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::Post)
([&db](const crow::request& req) {
    auto j = crow::json::load(req.body);
    if (!j)
        return crow::response(400, "Invalid JSON");

    if (!j.has("email") || !j.has("password") || !j.has("display_name"))
        return crow::response(400, "Missing field");

    const std::string email = std::string(j["email"].s());
    const std::string password = std::string(j["password"].s());
    const std::string display_name = std::string(j["display_name"].s());

    if (email.empty() || password.empty() || display_name.empty())
        return crow::response(400, "Empty fields");

    sqlite3* conn = db.handle();
    if (!conn)
        return crow::response(500, "Database handle is null");

    std::string salt = random_salt_hex();
    std::string pwd_hash = hash_password(password, salt);
    const long long ts = now_unix();

    // username temporaire unique
    std::string username = "__tmp__" + std::to_string(ts);

    const char* insert_sql =
        "INSERT INTO users(username,email,salt,password_hash,display_name,created_at) "
        "VALUES(?,?,?,?,?,?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(conn, insert_sql, -1, &st, nullptr) != SQLITE_OK)
        return crow::response(500, std::string("Prepare insert failed: ") + sqlite3_errmsg(conn));

    sqlite3_bind_text(st, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, pwd_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 5, display_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 6, static_cast<sqlite3_int64>(ts));

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE)
        return crow::response(500, std::string("DB insert failed: ") + sqlite3_errmsg(conn));

    long long user_id = sqlite3_last_insert_rowid(conn);
    std::string auto_username = std::to_string(user_id);

    const char* update_sql =
        "UPDATE users SET username = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(conn, update_sql, -1, &stmt, nullptr) != SQLITE_OK)
        return crow::response(500, std::string("Prepare update failed: ") + sqlite3_errmsg(conn));

    sqlite3_bind_text(stmt, 1, auto_username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(user_id));

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return crow::response(500, std::string("Update username failed: ") + sqlite3_errmsg(conn));
    }

    sqlite3_finalize(stmt);

    crow::json::wvalue res;
    res["ok"] = true;
    res["id"] = static_cast<int>(user_id);
    res["username"] = auto_username;
    res["email"] = email;
    res["display_name"] = display_name;

    return crow::response(200, res);
});

    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::Post)([](const crow::request &req)
                                                              {

    if (!db_init())
        return crow::response(500, "DB not ready");

    auto j = crow::json::load(req.body);
    if (!j) return crow::response(400, "Invalid JSON");

    if (!j.has("username_or_email") || !j.has("password"))
        return crow::response(400, "Missing fields");

    std::string id  = std::string(j["username_or_email"].s());
    std::string pwd = std::string(j["password"].s());

    if (id.empty() || pwd.empty())
        return crow::response(400, "Empty fields");

    // 1) Récupère user + salt + hash depuis SQLite
    auto uopt = db_get_user_auth_by_id(id);
    if (!uopt)
        return crow::response(401, "invalid credentials");

    const auto& u = *uopt;

    // 2) Vérifie le hash
    const std::string input_hash = hash_password(pwd, u.salt);
    if (input_hash != u.password_hash)
        return crow::response(401, "invalid credentials");

    // 3) Session token
    const std::string token = make_token();
    g_sessions[token] = u.username;

    // 4) Presence
    g_presence[u.username] = "online";

    crow::json::wvalue res;
    res["ok"] = true;
    res["token"] = token;
    res["username"] = u.username;
    return crow::response(200, res); });

    CROW_ROUTE(app, "/me").methods(crow::HTTPMethod::Get)([](const crow::request &req)
                                                          {
        
        const std::string token = get_bearer_token(req);
        if (token.empty()) return crow::response(401, "Missing bearer token");

        auto it = g_sessions.find(token);
        if (it == g_sessions.end()) return crow::response(401, "Invalid token");

        crow::json::wvalue res;
        res["ok"] = true;
        res["username"] = it->second;
        return crow::response(200, res); });

    CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::Post)([](const crow::request &req)
                                                               {
    const std::string token = get_bearer_token(req);
    if (token.empty()) return crow::response(401, "Missing bearer token");

    auto it = g_sessions.find(token);
    if (it == g_sessions.end()) return crow::response(401, "Invalid token");

    const std::string username = it->second;
    g_sessions.erase(it);

    // ✅ présence
    g_presence[username] = "offline";

    return crow::response(200, "logged out"); });

    CROW_ROUTE(app, "/signin").methods(crow::HTTPMethod::Post)([](const crow::request &req)
                                                               {
        std::cout << "[POST /signin] body:\n" << req.body << std::endl;
        return crow::response(200, "signin placeholder"); });

    CROW_ROUTE(app, "/presence").methods(crow::HTTPMethod::Get)([](const crow::request &req)
                                                                {
    std::string username;
    if (!require_auth(req, username))
        return crow::response(401, "Unauthorized");

    crow::json::wvalue res;
    res["ok"] = true;
    res["you"] = username;

    // ✅ fallback si pas encore défini
    auto it = g_presence.find(username);
    res["status"] = (it != g_presence.end()) ? it->second : "offline";

    return crow::response(200, res); });
}
