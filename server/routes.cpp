#include "routes.hpp"

void register_routes(crow::SimpleApp& app)
{
    CROW_ROUTE(app, "/kiki")
    ([]() {
        return "le plus beau de tout les kikis";
    });

    
    CROW_ROUTE(app, "/ping")
    ([]() {
        return "pong";
    });
}
