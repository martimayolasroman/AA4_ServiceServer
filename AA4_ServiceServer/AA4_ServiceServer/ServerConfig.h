#pragma once
#include <string>

struct ServerConfig {
    // Puertos
    unsigned short ServerPort = 55000;


    // Base de Datos
    std::string dbHost = "127.0.0.1";
    std::string dbUser = "root";
    std::string dbPass = "1234";
    std::string dbName = "DuckGameDB";

    // Launcher
    std::string mapFilePath = "Data/map.txt"; // Ruta relativa al ejecutable

    // Matchmaking
    std::string gameServerIp = "127.0.0.1";
    unsigned short gameServerDefaultPort = 56000;
    float matchmakingCheckInterval = 2.0f; // Segundos

    // Podrías añadir un método para cargar desde archivo aquí
    // bool loadFromFile(const std::string& filename);
};