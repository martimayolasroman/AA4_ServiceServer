#pragma once

#include <SFML/Network.hpp>
#include <list>
#include <string>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include "ClientSession.h"
#include "Utils.h"
#include "ServerTCP.h"
#include "ServerConfig.h"
// QUITAR: #include "LauncherService.h" // Ya no se necesita aquí

enum AdminPacketTypeDSSide {
    NOTIFY_NEW_GAME_DS = 200
};

inline sf::Packet& operator<<(sf::Packet& packet, AdminPacketTypeDSSide type) {
    return packet << static_cast<int>(type);
}

class MatchmakingService {
public:
    MatchmakingService(const ServerConfig& server_config); // Constructor ya no toma LauncherService
    ~MatchmakingService();

    void addClientToQueue(ClientSession& session, ServerTCP& tcpLayer);
    void formMatches(std::unordered_map<sf::TcpSocket*, ClientSession>& clientSessions,
        std::mutex& sessionsMutex,
        ServerTCP& tcpLayer);

    void attemptReconnectToDedicatedServerAdmin();

private:
    const ServerConfig& config;
    // QUITAR: LauncherService& launcher;
    sf::TcpSocket dedicated_server_admin_socket;
    std::mutex admin_socket_mutex;

    std::list<sf::TcpSocket*> matchmakingQueue;
    std::mutex queueMutex;

    unsigned short assignNextGameClientUdpPort();
    std::string generateRoomId();
    bool connectToDedicatedServerAdmin();
};