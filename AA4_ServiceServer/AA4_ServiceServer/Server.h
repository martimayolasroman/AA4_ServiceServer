#pragma once

#include "ServerTCP.h"
#include "DBManager.h"
#include "ClientSession.h"
#include "Utils.h"
#include "ServerConfig.h"
#include <unordered_map>
#include <list>
#include <atomic>
#include <thread>
#include <mutex>
#include <fstream>
#include <vector>
#include "LauncherService.h"
#include "MatchmakingService.h"
#include "AuthService.h"

class Server {
public:
    Server(const ServerConfig& config);
    ~Server();

    void run();
    void stop();

private:
    void handleClientConnected(sf::TcpSocket* clientSocket);
    void handleClientDisconnected(sf::TcpSocket* clientSocket);
    void processPacket(sf::TcpSocket* client, sf::Packet& packet);

    void matchmakingThreadLoop();
    // void checkQueueAndFormMatches(); // Esta lógica está en MatchmakingService::formMatches

    ServerTCP serverTCP;
    DBManager dbManager;
    ServerConfig serverConfig;

    // Módulos de Lógica
    LauncherService launcher; // Sigue aquí, pero no se usa para enviar mapa al inicio
    AuthService auth;
    MatchmakingService matchmaking; // Constructor modificado

    std::unordered_map<sf::TcpSocket*, ClientSession> clientSessions;
    std::mutex sessionsMutex;

    // Matchmaking Queue (la gestiona MatchmakingService ahora)
    // std::list<sf::TcpSocket*> matchmakingQueue;
    // std::mutex matchmakingMutex;
    std::thread mmThread;

    std::atomic<bool> matchmaking_running_flag;
    std::atomic<bool> server_running_flag;
};