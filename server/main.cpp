// #include "crow_all.h"
// #include "routes.hpp"

// int main()
// {
//     crow::SimpleApp app;

//     register_routes(app);

//     app.bindaddr("127.0.0.1")
//        .port(18080)
//        .run();

//     return 0;
// }
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
