#include "MatchmakingService.h"
#include <iostream>
#include <optional>

static unsigned short next_game_client_udp_port_val = 57000;
static std::mutex port_assignment_mutex_val;

unsigned short MatchmakingService::assignNextGameClientUdpPort() {
    std::lock_guard<std::mutex> lock(port_assignment_mutex_val);
    unsigned short assigned_port = next_game_client_udp_port_val;
    next_game_client_udp_port_val++;
    if (next_game_client_udp_port_val > 57100) {
        next_game_client_udp_port_val = 57000;
    }
    return assigned_port;
}

std::string MatchmakingService::generateRoomId() {
    static int room_counter = 0;
    std::ostringstream oss;
    oss << "ds_room_" << room_counter++;
    return oss.str();
}

bool MatchmakingService::connectToDedicatedServerAdmin() {
    if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any) {
        dedicated_server_admin_socket.disconnect();
    }

    std::optional<sf::IpAddress> resolved_ip_opt = sf::IpAddress::resolve(config.gameServerIp);
    if (!resolved_ip_opt || resolved_ip_opt.value() == sf::IpAddress::Any) {
        std::cerr << "[MatchmakingService] ERROR: IP resuelta para DedicatedServer Admin es invalida o no se pudo resolver: " << config.gameServerIp << std::endl;
        return false;
    }

    if (dedicated_server_admin_socket.connect(resolved_ip_opt.value(), config.gameServerDefaultPortTCPAdmin) != sf::Socket::Status::Done) {
        std::cerr << "[MatchmakingService] ERROR: No se pudo conectar al puerto de admin del DedicatedServer ("
            << config.gameServerIp << ":" << config.gameServerDefaultPortTCPAdmin << ")" << std::endl;
        return false;
    }
    std::cout << "[MatchmakingService] Conectado al puerto de admin del DedicatedServer." << std::endl;
    dedicated_server_admin_socket.setBlocking(true);
    return true;
}

void MatchmakingService::attemptReconnectToDedicatedServerAdmin() {
    std::cout << "[MatchmakingService] Intentando reconectar al puerto admin del DedicatedServer..." << std::endl;
    connectToDedicatedServerAdmin();
}

// Constructor ya no toma LauncherService
MatchmakingService::MatchmakingService(const ServerConfig& server_config)
    : config(server_config) {
    connectToDedicatedServerAdmin();
}

MatchmakingService::~MatchmakingService() {
    if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any) {
        dedicated_server_admin_socket.disconnect();
        std::cout << "[MatchmakingService] Desconectado del puerto admin del DedicatedServer." << std::endl;
    }
}

void MatchmakingService::addClientToQueue(ClientSession& session, ServerTCP& tcpLayer) {
    if (session.state != ClientState::LOGGED_IN) {
        std::cerr << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName()
            << " intento entrar a matchmaking sin estar LOGGED_IN. Estado actual: "
            << static_cast<int>(session.state) << std::endl;
        return;
    }

    std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName()
        << " (" << session.ipAddress.toString() << ":" << session.port
        << ") solicita matchmaking." << std::endl;

    // QUITAR env�o de mapa
     //std::cout << "[MatchmakingLogic] Enviando mapa a " << session.playerInfo.getNickName() << std::endl;
     //launcher.sendMapToClient(session, tcpLayer);

    bool added_to_actual_queue = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        bool alreadyInQueue = false;

        // Comprueba si el socket ya est� en la cola para evitar duplicados
        for (sf::TcpSocket* queuedSocket : matchmakingQueue) {
            if (queuedSocket == session.socket) {
                alreadyInQueue = true;
                break;
            }
        }

        
        if (!alreadyInQueue) {
            matchmakingQueue.push_back(session.socket);
            session.state = ClientState::IN_MATCHMAKING_QUEUE; // Actualizar estado AHORA
            added_to_actual_queue = true;
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName() << " anadido a la lista de matchmaking." << std::endl;
        }
        else {
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName() << " ya estaba en la lista de matchmaking." << std::endl;
        }
    }

    if (added_to_actual_queue) {
        sf::Packet response;
        response << PacketType::S_ADDED_TO_MATCHMAKING_QUEUE;
        if (tcpLayer.sendToClient(session.socket, response)) {
            std::cout << "[MatchmakingLogic] Notificacion S_ADDED_TO_MATCHMAKING_QUEUE enviada a " << session.playerInfo.getNickName() << std::endl;
        }
        else {
            std::cerr << "[MatchmakingLogic] Error enviando S_ADDED_TO_MATCHMAKING_QUEUE a " << session.playerInfo.getNickName() << std::endl;
            session.state = ClientState::LOGGED_IN;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                matchmakingQueue.remove(session.socket);
            }
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName() << " eliminado de la cola debido a error de envio." << std::endl;
        }
    }
}

void MatchmakingService::formMatches(std::unordered_map<sf::TcpSocket*, ClientSession>& clientSessionsMap, std::mutex& sessionsMutexRef, ServerTCP& tcpLayer) {
    std::lock_guard<std::mutex> lock(queueMutex);

    if (matchmakingQueue.size() < 2) {
        return;
    }

    sf::TcpSocket* socket1_ptr = matchmakingQueue.front();
    matchmakingQueue.pop_front();
    sf::TcpSocket* socket2_ptr = matchmakingQueue.front();
    matchmakingQueue.pop_front();

    ClientSession* session1 = nullptr;
    ClientSession* session2 = nullptr;

    {
        std::lock_guard<std::mutex> sessionsLock(sessionsMutexRef);
        auto it1 = clientSessionsMap.find(socket1_ptr);
        if (it1 != clientSessionsMap.end()) session1 = &it1->second;
        auto it2 = clientSessionsMap.find(socket2_ptr);
        if (it2 != clientSessionsMap.end()) session2 = &it2->second;
    }

    if (!session1 || !session2) {
        std::cerr << "[MatchmakingLogic] Error: No se encontro la sesion para uno o ambos sockets de la cola." << std::endl;
        if (socket1_ptr && session1 && session1->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket1_ptr);
        if (socket2_ptr && session2 && session2->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket2_ptr);
        return;
    }

    if (session1->state != ClientState::IN_MATCHMAKING_QUEUE || session2->state != ClientState::IN_MATCHMAKING_QUEUE) {
        std::cerr << "[MatchmakingLogic] Error: Uno o ambos jugadores emparejados no estaban en estado IN_MATCHMAKING_QUEUE." << std::endl;
        if (session1->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket1_ptr);
        else session1->state = ClientState::LOGGED_IN;
        if (session2->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket2_ptr);
        else session2->state = ClientState::LOGGED_IN;
        return;
    }

    std::cout << "[MatchmakingLogic] Emparejando " << session1->playerInfo.getNickName()
        << " con " << session2->playerInfo.getNickName() << std::endl;

    unsigned short client1_game_udp_port = assignNextGameClientUdpPort();
    unsigned short client2_game_udp_port = assignNextGameClientUdpPort();
    std::string new_room_id = generateRoomId();

    sf::Packet notification_to_dedicated;
    notification_to_dedicated << AdminPacketTypeDSSide::NOTIFY_NEW_GAME_DS;
    notification_to_dedicated << new_room_id
        << session1->ipAddress.toString()
        << client1_game_udp_port
        << session2->ipAddress.toString()
        << client2_game_udp_port;

    bool notified_dedicated_server = false;
    {
        std::lock_guard<std::mutex> admin_sock_lock(admin_socket_mutex);
        if (dedicated_server_admin_socket.getRemoteAddress() == sf::IpAddress::Any) {
            std::cout << "[MatchmakingService] Socket de admin no conectado. Intentando reconectar..." << std::endl;
            if (!connectToDedicatedServerAdmin()) {
                std::cerr << "[MatchmakingService] Fallo al reconectar con el DS admin. No se puede notificar nueva partida." << std::endl;
                matchmakingQueue.push_front(socket2_ptr);
                matchmakingQueue.push_front(socket1_ptr);
                return;
            }
        }

        if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any) {
            if (dedicated_server_admin_socket.send(notification_to_dedicated) == sf::Socket::Status::Done) {
                std::cout << "[MatchmakingService] Notificacion enviada al DedicatedServer para la sala " << new_room_id << std::endl;
                notified_dedicated_server = true;
            }
            else {
                std::cerr << "[MatchmakingService] ERROR: No se pudo enviar notificacion al DedicatedServer. Intentara reconectar la proxima vez." << std::endl;
                dedicated_server_admin_socket.disconnect();
            }
        }
        else {
            std::cerr << "[MatchmakingService] Socket de admin sigue sin estar conectado tras intento. No se puede notificar." << std::endl;
        }
    }

    if (!notified_dedicated_server) {
        std::cerr << "[MatchmakingLogic] Fallo al notificar al Dedicated Server. Devolviendo jugadores a la cola." << std::endl;
        matchmakingQueue.push_front(socket2_ptr);
        matchmakingQueue.push_front(socket1_ptr);
        return;
    }

    bool client1_is_player_one = true;
    bool client2_is_player_one = false;

    sf::Packet matchPacket1;
    matchPacket1 << PacketType::S_MATCH_FOUND
        << config.gameServerIp
        << config.gameServerDefaultPort
        << client1_game_udp_port
        << client1_is_player_one;

    sf::Packet matchPacket2;
    matchPacket2 << PacketType::S_MATCH_FOUND
        << config.gameServerIp
        << config.gameServerDefaultPort
        << client2_game_udp_port
        << client2_is_player_one;

    bool sent1 = tcpLayer.sendToClient(session1->socket, matchPacket1);
    bool sent2 = tcpLayer.sendToClient(session2->socket, matchPacket2);

    if (sent1 && sent2) {
        session1->state = ClientState::MATCHED;
        session2->state = ClientState::MATCHED;
        std::cout << "[MatchmakingLogic] Notificacion S_MATCH_FOUND enviada a ambos jugadores." << std::endl;
    }
    else {
        std::cerr << "[MatchmakingLogic] Error enviando S_MATCH_FOUND a uno o ambos jugadores." << std::endl;
        if (!sent1 && session1) {
            session1->state = ClientState::LOGGED_IN;
            matchmakingQueue.push_front(socket1_ptr);
            std::cout << "[MatchmakingLogic] Jugador 1 devuelto a la cola." << std::endl;
        }
        if (!sent2 && session2) {
            session2->state = ClientState::LOGGED_IN;
            matchmakingQueue.push_front(socket2_ptr);
            std::cout << "[MatchmakingLogic] Jugador 2 devuelto a la cola." << std::endl;
        }
    }
}