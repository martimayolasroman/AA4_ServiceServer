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

    // Launchervis
    std::string mapFilePath = "Data/map.txt"; // Ruta relativa al ejecutable

    // Matchmaking
    std::string gameServerIp = "192.168.1.46"; //Ip server DEDICADO
    unsigned short gameServerDefaultPort = 55002; // PUERTO SERVER DEDICADO
    unsigned short gameServerDefaultPortTCPAdmin = 55003; //PUERTO SERVER DEDICADO PARA TCP
    float matchmakingCheckInterval = 2.0f; // Segundos

   
};