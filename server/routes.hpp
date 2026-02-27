#pragma once
#include "crow_all.h"

class Database; // forward declaration

// Enregistre toutes les routes HTTP du serveur
void register_routes(crow::SimpleApp& app, Database& db);
