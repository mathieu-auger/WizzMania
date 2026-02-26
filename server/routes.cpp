#include "routes.hpp"

void register_routes(crow::SimpleApp& app)
{
    CROW_ROUTE(app, "/chat")
    ([]() {
        return "Le chat dit: miaou!";
    });

    
    CROW_ROUTE(app, "/ping")
    ([]() {
        return "pong";
    });
}
