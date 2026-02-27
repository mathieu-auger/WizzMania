#include "crow_all.h"
#include "routes.hpp"
#include <filesystem>
#include "database.hpp"
#include <iostream>


// ...


int main()
{
    crow::SimpleApp app;

    Database db("app.db");

    if (!db.init()) {
        std::cerr << "Database init failed\n";
        return 1;
    }

    // 🔹 on injecte la db dans les routes
    register_routes(app, db);

    std::cout << "CWD = "
              << std::filesystem::current_path()
              << std::endl;

    app.port(18080).multithreaded().run();

    return 0;
}