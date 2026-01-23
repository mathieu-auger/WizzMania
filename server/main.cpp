#include "crow_all.h"
#include "routes.hpp"

int main()
{
    crow::SimpleApp app;

    register_routes(app);

    app.bindaddr("127.0.0.1")
       .port(18080)
       .run();

    return 0;
}
