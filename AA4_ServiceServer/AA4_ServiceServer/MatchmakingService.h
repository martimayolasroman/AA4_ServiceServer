#pragma once
#include <list>
#include <string>
#include <mutex> // Para proteger la cola si se accede desde múltiples contextos 
#include "ClientSession.h"
#include "Utils.h"
#include "ServerTCP.h"
#include "ServerConfig.h" // Para gameServerIp y port



class MatchmakingService
{

public:
    MatchmakingService(const ServerConfig& server_config);
   
    // Añade un cliente a la cola de matchmaking.
    // Devuelve true si se añadió, false si ya estaba o hubo otro problema.
    // Actualiza session.state.
    // Envía S_ADDED_TO_MATCHMAKING_QUEUE al cliente.
    void addClientToQueue(ClientSession& session, ServerTCP& tcpLayer);

    // Revisa la cola y forma parejas si es posible.
   // Esta función será llamada periódicamente por el thread de matchmaking en Server.
   // Necesita acceso al mapa de sesiones (para cambiar estado) y a ServerTCP (para enviar S_MATCH_FOUND).
    void formMatches(std::unordered_map<sf::TcpSocket*, ClientSession>& clientSessions,
        std::mutex& sessionsMutex, // Para proteger clientSessions
        ServerTCP& tcpLayer);

private:

    std::list<sf::TcpSocket*> matchmakingQueue; // Almacena punteros a sockets
    std::mutex queueMutex; // Protege matchmakingQueue
    const ServerConfig& config; // Referencia a la configuración del servidor


};

