#pragma once
#include "crow_all.h"

class Database; // forward declaration

void register_routes(crow::SimpleApp& app, Database& db);
