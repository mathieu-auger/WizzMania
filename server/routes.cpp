#include "crow_all.h"
#include "routes.hpp"

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

// ===== mémoire =====
static std::unordered_map<std::string, std::string> g_sessions;
static std::mutex g_db_mtx;
static std::unordered_set<std::string> g_usernames;
static std::unordered_set<std::string> g_emails;
static std::unordered_map<std::string, std::string> g_presence; // username -> status
static std::unordered_map<std::string, std::vector<crow::websocket::connection*>> g_ws_by_user;
static std::unordered_map<crow::websocket::connection*, std::string> g_user_by_ws;
static std::mutex g_ws_mtx;

static void ws_send_to_user(const std::string& username,
                            const std::string& payload)
                            {
    std::lock_guard<std::mutex> lock(g_ws_mtx);

    auto it = g_ws_by_user.find(username);
    if (it == g_ws_by_user.end()) return;

    for (auto* c : it->second)
    {
        if (c) c->send_text(payload);
    }
}

static std::string get_query_param(const crow::request& req, const std::string& key)
{
    auto v = req.url_params.get(key);
    return v ? std::string(v) : std::string{};
}

static void ws_add(const std::string& user, crow::websocket::connection& conn)
{
    std::lock_guard<std::mutex> lock(g_ws_mtx);
    g_ws_by_user[user].push_back(&conn);
    g_user_by_ws[&conn] =user;
}

static void ws_remove(crow::websocket::connection& conn)
{
    std::lock_guard<std::mutex> lock(g_ws_mtx);

    auto itU = g_user_by_ws.find(&conn);
    if (itU == g_user_by_ws.end()) return;

    const std::string user = itU->second;
    g_user_by_ws.erase(itU);

    auto it = g_ws_by_user.find(user);
    if (it != g_ws_by_user.end())
    {
        auto& vec = it->second;

        vec.erase(
            std::remove(vec.begin(), vec.end(), &conn),
            vec.end()
        );

        if (vec.empty())
            g_ws_by_user.erase(it);
    }

    std::cout << "[WS] disconnected: " << user << std::endl;
}


static const std::string USERS_PATH = "users.jsonl";
static sqlite3* g_db =nullptr;
static const char* DB_PATH = "app.db";
static crow::response db_prepare_error(const char* where)
{
    const char* msg = (g_db ? sqlite3_errmsg(g_db) : "g_db is null");
    std::cout << "[DB] " << where << " prepare failed: " << msg << "\n";
    return crow::response(500, std::string("DB prepare failed: ") + msg);
}

static crow::response db_step_error(const char* where, int rc)
{
    const char* msg = (g_db ? sqlite3_errmsg(g_db) : "g_db is null");
    std::cout << "[DB] " << where << " step failed rc=" << rc << ": " << msg << "\n";
    return crow::response(500, std::string("DB step failed: ") + msg);
}

static bool init_db()
{
    int rc = sqlite3_open_v2(DB_PATH, &g_db,
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "sqlite open failed:" << (g_db ? sqlite3_errmsg(g_db) : "null") << "/n";
        return false;
    }

    sqlite3_busy_timeout(g_db, 2000);
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    return true;
}

static bool db_exec(const char* sql)
{
    char* err = nullptr;

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
    static std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_int_distribution<unsigned long long> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

static std::string get_bearer_token(const crow::request& req)
{
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) return {};

    const std::string& v = it->second;
    const std::string prefix = "Bearer ";
    if (v.rfind(prefix, 0) != 0) return {};
    return v.substr(prefix.size());
}

static bool require_auth(const crow::request& req, std::string& out_username)
{
    const std::string token = get_bearer_token(req);
    if (token.empty()) return false;

    auto it = g_sessions.find(token);
    if (it == g_sessions.end()) return false;

    out_username = it->second;
    return true;
}


static bool append_line(const std::string& path, const std::string& line)
{
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) return false;
    out << line << "\n";
    return true;
}

// ===== hash + salt (POC sérieux) =====
static std::string random_salt_hex(size_t bytes = 16)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(0, 255);

    std::ostringstream oss;
    for (size_t i = 0; i < bytes; ++i) {
        int b = dist(rng);
        oss << std::hex << std::setw(2) << std::setfill('0') << (b & 0xff);
    }
    return oss.str();
}

static std::string sha256_hex(const std::string& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

static std::string hash_password(const std::string& password, const std::string& salt_hex)
{
    return sha256_hex(salt_hex + ":" + password);
}

// ===== fichier users.jsonl =====
static void load_users_from_file(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cout << "[LOAD] No existing file: " << path << " (first run)\n";
        return;
    }

    std::string line;
    int loaded = 0;
    int skipped = 0;

    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        auto j = crow::json::load(line);
        if (!j) { skipped++; continue; }

        // sécurité: ne jamais indexer sans has()
        if (!j.has("username") || !j.has("email")) { skipped++; continue; }

        std::string username = std::string(j["username"].s());
        std::string email    = std::string(j["email"].s());

        if (username.empty() || email.empty()) { skipped++; continue; }

        g_usernames.insert(username);
        g_emails.insert(email);
        loaded++;
    }

    std::cout << "[LOAD] users loaded=" << loaded << " skipped=" << skipped << " from " << path << "\n";
}

static bool check_credentials_in_file(const std::string& path,
                                      const std::string& username_or_email,
                                      const std::string& password_input,
                                      std::string& out_username)
{
    std::ifstream in(path);
    std::cout << "[LOGIN] open '" << path << "' -> " << (in.is_open() ? "OK" : "FAIL") << "\n";
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        auto u = crow::json::load(line);
        if (!u) continue;

        if (!u.has("username") || !u.has("email")) continue;

        std::string username = std::string(u["username"].s());
        std::string email    = std::string(u["email"].s());

        const bool id_match = (username_or_email == username) || (username_or_email == email);
        if (!id_match) continue;

        // --- mode hash ---
        std::string salt, stored_hash;
        if (u.has("salt")) salt = std::string(u["salt"].s());
        if (u.has("password_hash")) stored_hash = std::string(u["password_hash"].s());

        if (!salt.empty() && !stored_hash.empty()) {
            const std::string input_hash = hash_password(password_input, salt);
            if (input_hash == stored_hash) { out_username = username; return true; }
            return false;
        }

        // --- fallback mode dev ---
        std::string stored_pwd;
        if (u.has("password")) stored_pwd = std::string(u["password"].s());

        if (!stored_pwd.empty()) {
            if (stored_pwd == password_input) { out_username = username; return true; }
            return false;
        }

        return false;
    }

    return false;
}

// ====SQLite ====
static void normalize_pair(std::string& a, std::string& b)
{
    if (b < a) std::swap(a, b);
}


static bool db_init()
{
    if (g_db) return true;

    if (sqlite3_open(DB_PATH, &g_db) != SQLITE_OK) {
        std::cout << "[DB] cannot open " << DB_PATH << " : " << sqlite3_errmsg(g_db) << "\n";
        return false; // IMPORTANT
    }

    db_exec("PRAGMA journal_mode=WAL;");
    db_exec("PRAGMA synchronous=NORMAL;");

    const char* schema = R"SQL(
    PRAGMA foreign_keys = ON;

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

    if (!db_exec(schema)) return false;

    std::cout << "[DB] ready: " << DB_PATH << "\n";
    return true;
}


static void db_close()
    {
        if (g_db) {
            sqlite3_close(g_db);
            g_db = nullptr;
        }
    }

    static bool db_are_friends(const std::string& u1, const std::string& u2)
    {
        std::string a =u1, b = u2;
        normalize_pair(a, b);

        const char* sql = "SELECT 1 FROM friends WHERE user_a=? AND user_b=?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(st, 1, a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, b.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = (sqlite3_step(st) == SQLITE_ROW);
        sqlite3_finalize(st);
        return ok;
    }

    static bool db_request_exists(const std::string& from, const std::string& to)
    {
        const char* sql = "SELECT 1 FROM friend_requests WHERE from_user=? AND to_user=?;";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK) return false;

        sqlite3_bind_text(st, 1, from.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, from.c_str(), -1, SQLITE_TRANSIENT);

        bool ok = (sqlite3_step(st) == SQLITE_ROW);
        sqlite3_finalize(st);
        return ok;
    }

// ===== ROUTES =====
void register_routes(crow::SimpleApp& app, Database& db)
{
     // fichier dans build/server/
    if (!db_init()) {
        std::cout << "[DB] init failed\n";
    }
    load_users_from_file(USERS_PATH);

        CROW_WEBSOCKET_ROUTE(app, "/ws")
    .onaccept([&](const crow::request& req,
              std::optional<crow::response>& res,
              void** userdata) {

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
    *userdata = new std::string(it->second);
    })
    .onopen([&](crow::websocket::connection& conn) {
    auto* u = static_cast<std::string*>(conn.userdata());
    if (!u) return;

    const std::string& username = *u;

    std::lock_guard<std::mutex> lock(g_ws_mtx);
    g_ws_by_user[username].push_back(&conn);
    g_user_by_ws[&conn] = username;

    std::cout << "[WS] open user=" << username << "\n";
    })
    .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
    (void)is_binary;

    auto* u = static_cast<std::string*>(conn.userdata());
    if (!u) return;

    const std::string& from = *u;
    std::cout << "[WS] msg from=" << from << " data=" << data << "\n";
    })
    .onclose([&](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
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

    std::cout << "[WS] close user=" << username << "\n";
    });

    CROW_ROUTE(app, "/dm/send").methods(crow::HTTPMethod::Post)
([](const crow::request& req){
    std::string from;
    if (!require_auth(req, from))
        return crow::response(401, "Unauthorized");

    auto j = crow::json::load(req.body);
    if (!j) return crow::response(400, "Invalid JSON");
    if (!j.has("to") || !j.has("text"))
        return crow::response(400, "Missing field");

    const std::string to   = std::string(j["to"].s());
    const std::string text = std::string(j["text"].s());
    if (to.empty() || text.empty())
        return crow::response(400, "Empty fields");

    // ✅ FIX: Unknown user uniquement si to n'existe pas
    if (!g_usernames.count(to))
        return crow::response(404, "Unknown user");

    const char* sql =
        "INSERT INTO messages(from_user, to_user, body, created_at) VALUES(?,?,?,?);";
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
        return db_prepare_error("dm: insert");

    const long long ts = now_unix();

    // ✅ FIX: bind correct
    sqlite3_bind_text(st, 1, from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, to.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)ts);

    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return crow::response(500, std::string("DB insert failed: ") + sqlite3_errmsg(g_db));
    }
    sqlite3_finalize(st);

    const long long msg_id = (long long)sqlite3_last_insert_rowid(g_db);

    crow::json::wvalue payload;
    payload["type"] = "dm";
    payload["id"]   = msg_id;
    payload["from"] = from;
    payload["to"]   = to;
    payload["text"] = text;
    payload["ts"]   = ts;

    // envoyer au destinataire si connecté en websocket
    ws_send_to_user(to, payload.dump());

    // ✅ retourne un ACK au client HTTP
    crow::json::wvalue ack;
    ack["ok"] = true;
    ack["id"] = msg_id;
    ack["ts"] = ts;
    return crow::response(200, ack);
    });

    CROW_ROUTE(app, "/dm/history").methods(crow::HTTPMethod::Get)
    ([&](const crow::request& req){
        std::string me;
        if (!require_auth(req, me)) return crow::response(401);

        const char* other = req.url_params.get("with");
        if (!other) return crow::response(400, "Missing 'with'");

        sqlite3_stmt* stmt = nullptr;
        const char* sql =
        "SELECT id, from_user, to_user, body, created_at, seen_at"
        "FROM messages"
        "WHERE (from_user=? AND to_user=?) OR (from_user=? AND to_user=?)"
        "ORDER BY id DESC LIMIT 50;";

        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return crow::response(500, "DB prepare failed");

        sqlite3_bind_text(stmt, 1, me.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, other, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, other, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, other, -1, SQLITE_TRANSIENT),
        sqlite3_bind_text(stmt, 4, me.c_str(), -1,SQLITE_TRANSIENT);


        crow::json::wvalue out;
        int i = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            crow::json::wvalue m;
            m["id"] = sqlite3_column_int(stmt, 0);
            m["from"] = (const char*)sqlite3_column_text(stmt, 1);
            m["to"] = (const char*)sqlite3_column_text(stmt, 2);
            m["body"] = (const char*)sqlite3_column_text(stmt, 3);
            m["created_at"] = sqlite3_column_int64(stmt, 4);

            if (sqlite3_column_type(stmt, 5) == SQLITE_NULL) m["seen_at"] = nullptr;
            else m["seen_at"] = sqlite3_column_int64(stmt,5);

            out[i++] = std::move(m);
        }

        sqlite3_finalize(stmt);
        return crow::response(out);
    });

    CROW_ROUTE(app, "/friends").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req){
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
    return crow::response(200, res);
    });

    CROW_ROUTE(app, "/friends/request").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){

        //auth
        std::string me;
        if (!require_auth(req, me))
        return crow::response(401, "Unauthorized");

        //db
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

        //user must exist
        if (g_usernames.find(to) == g_usernames.end())
        return crow::response(404, "User not found");

        //already friends?
        if (db_are_friends(me, to))
        return crow::response(409, "Already friends");

        //already sent ?

        if (db_request_exists(me, to))
        return crow::response(409,"Request already sent");

        //if reverse request exists
        if (db_request_exists(to, me))
        return crow::response(409, "Incoming request exist. Use /friends/accept");

        //insert request
        const char* sql =
    "INSERT OR IGNORE INTO friend_requests(from_user,to_user,created_at) VALUES(?,?,?);";

    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, nullptr) != SQLITE_OK)
    return db_prepare_error("friends/request insert");
std::cout << me.c_str()<< std::endl;
std::cout << to.c_str()<< std::endl;

    sqlite3_bind_text(st, 1, me.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, to.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)now_unix());

    int rc = sqlite3_step(st);
    sqlite3_finalize(st);

    if (rc != SQLITE_DONE)
    return db_step_error("friends/request insert", rc);


            //response Json
            crow::json::wvalue res;
            res["ok"] = true;
            res["from"] = me;
            res["to"] = to;
            res["message"] = "request sent";
            return crow::response(200, res);

    });

    CROW_ROUTE(app, "/friends/accept").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
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
        return crow::response(200, res);
    });

    CROW_ROUTE (app, "/friend").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req){

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
        return crow::response(200, res);
    });



    CROW_ROUTE(app, "/ping").methods(crow::HTTPMethod::Get)
    ([] {
        return crow::response(200, "pong");
    });

    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req)  // <- Ajoutez le & pour capturer USERS_PATH
    {
    try {
        std::cout << "[POST /register] body:\n" << req.body << std::endl;

        auto j = crow::json::load(req.body);
        if (!j) return crow::response(400, "Invalid JSON");

        if (!j.has("username") || !j.has("email") || !j.has("password"))
            return crow::response(400, "Missing fields");

        std::string username = std::string(j["username"].s());
        std::string email    = std::string(j["email"].s());
        std::string password = std::string(j["password"].s());

        if (username.empty() || email.empty() || password.empty())
            return crow::response(400, "Empty fields");

        // VÉRIFIER LES DOUBLONS
        if (g_usernames.count(username))
            return crow::response(409, "Username already exists");
        if (g_emails.count(email))
            return crow::response(409, "Email already exists");

        // HASH LE MOT DE PASSE
        std::string salt = random_salt_hex();
        std::string pwd_hash = hash_password(password, salt);

        // UTILISER crow::json::wvalue pour créer le JSON proprement
        crow::json::wvalue user;
        user["username"] = username;
        user["email"] = email;
        user["salt"] = salt;
        user["password_hash"] = pwd_hash;

        std::string line = user.dump();

        if (!append_line(USERS_PATH, line))
            return crow::response(500, "Cannot write file");

        // METTRE À JOUR LA MÉMOIRE
        g_usernames.insert(username);
        g_emails.insert(email);

        std::cout << "[REGISTER] OK\n";
        return crow::response(201, "registered");
    }
    catch (const std::exception& e) {
        std::cout << "[REGISTER] EXCEPTION: " << e.what() << std::endl;
        return crow::response(500, "Internal error");
    }
    });


    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::Post)
([&](const crow::request& req) {
    std::cout << "[LOGIN] === DEBUT ===" << std::endl;

    try {
        std::cout << "[LOGIN] body:\n" << req.body << std::endl;

        auto j = crow::json::load(req.body);
        std::cout << "[LOGIN] JSON parsed OK" << std::endl;

        if (!j) {
            std::cout << "[LOGIN] JSON invalide" << std::endl;
            return crow::response(400, "Invalid JSON");
        }

        if (!j.has("username_or_email") || !j.has("password")) {
            std::cout << "[LOGIN] Champs manquants" << std::endl;
            return crow::response(400, "Missing fields");
        }

        std::string id  = std::string(j["username_or_email"].s());
        std::string pwd = std::string(j["password"].s());
        std::cout << "[LOGIN] id=" << id << " pwd=" << pwd << std::endl;

        if (id.empty() || pwd.empty()) {
            std::cout << "[LOGIN] Champs vides" << std::endl;
            return crow::response(400, "Empty fields");
        }

        std::cout << "[LOGIN] Avant check_credentials..." << std::endl;
        std::string username;
        bool ok = check_credentials_in_file(USERS_PATH, id, pwd, username);
        std::cout << "[LOGIN] check_credentials result=" << ok << std::endl;

        if (!ok) {
            std::cout << "[LOGIN] Credentials invalides" << std::endl;
            return crow::response(401, "invalid credentials");
        }

        const std::string token = make_token();
        g_sessions[token] = username;

        // ✅ présence: username -> status
        g_presence[username] = "online";

        std::cout << "[LOGIN] Token créé: " << token << std::endl;

        crow::json::wvalue res;
        res["ok"] = true;
        res["token"] = token;
        res["username"] = username;

        std::cout << "[LOGIN] === SUCCES ===" << std::endl;
        return crow::response(200, res);
    }
    catch (const std::exception& e) {
        std::cout << "[LOGIN] EXCEPTION: " << e.what() << std::endl;
        return crow::response(500, std::string("Error: ") + e.what());
    }
    catch (...) {
        std::cout << "[LOGIN] UNKNOWN EXCEPTION" << std::endl;
        return crow::response(500, "Unknown error");
    }
    });



    CROW_ROUTE(app, "/me").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {
        
        const std::string token = get_bearer_token(req);
        if (token.empty()) return crow::response(401, "Missing bearer token");

        auto it = g_sessions.find(token);
        if (it == g_sessions.end()) return crow::response(401, "Invalid token");

        crow::json::wvalue res;
        res["ok"] = true;
        res["username"] = it->second;
        return crow::response(200, res);
    });

    CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::Post)
([](const crow::request& req) {
    const std::string token = get_bearer_token(req);
    if (token.empty()) return crow::response(401, "Missing bearer token");

    auto it = g_sessions.find(token);
    if (it == g_sessions.end()) return crow::response(401, "Invalid token");

    const std::string username = it->second;
    g_sessions.erase(it);

    // ✅ présence
    g_presence[username] = "offline";

    return crow::response(200, "logged out");
});


    CROW_ROUTE(app, "/signin").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {
        std::cout << "[POST /signin] body:\n" << req.body << std::endl;
        return crow::response(200, "signin placeholder");
    });

    CROW_ROUTE(app, "/presence").methods(crow::HTTPMethod::Get)
    ([](const crow::request& req) {
    std::string username;
    if (!require_auth(req, username))
        return crow::response(401, "Unauthorized");

    crow::json::wvalue res;
    res["ok"] = true;
    res["you"] = username;

    // ✅ fallback si pas encore défini
    auto it = g_presence.find(username);
    res["status"] = (it != g_presence.end()) ? it->second : "offline";

    return crow::response(200, res);
});

}
