

#include "MatchmakingService.h"
#include <iostream>
#include <optional> 



static unsigned short next_game_client_udp_port_val = 55100; // Puerto UDP inicial para clientes en partida
static std::mutex port_assignment_mutex_val; // Mutex para proteger la asignación del puerto



// Obtiene el siguiente puerto UDP disponible de la secuencia.
unsigned short MatchmakingService::assignNextGameClientUdpPort() {
    std::lock_guard<std::mutex> lock(port_assignment_mutex_val); 
    unsigned short assigned_port = next_game_client_udp_port_val;
    next_game_client_udp_port_val++;
    
    if (next_game_client_udp_port_val > 55200) {
        next_game_client_udp_port_val = 55100;
    }
    return assigned_port;
}

// Crea un ID de sala único simple (e.g., "ds_room_0", "ds_room_1").
std::string MatchmakingService::generateRoomId() {
    static int room_counter = 0; 
    std::ostringstream oss;
    oss << "ds_room_" << room_counter++;
    return oss.str();
}

//Establece (o restablece) la conexión TCP con el puerto de administración del DedicatedServer.
bool MatchmakingService::connectToDedicatedServerAdmin() {
    // Si ya hay una conexión activa, desconectar primero
    if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any && dedicated_server_admin_socket.getRemoteAddress().has_value()) {
        dedicated_server_admin_socket.disconnect();
    }

    std::optional<sf::IpAddress> resolved_ip_opt = sf::IpAddress::resolve(config.gameServerIp);
    if (!resolved_ip_opt || resolved_ip_opt.value() == sf::IpAddress::Any) {
        std::cerr << "[MatchmakingService] ERROR: IP resuelta para DedicatedServer Admin es invalida o no se pudo resolver: " << config.gameServerIp << std::endl;
        return false;
    }

    sf::Socket::Status status = dedicated_server_admin_socket.connect(resolved_ip_opt.value(), config.gameServerDefaultPortTCPAdmin);
    if (status != sf::Socket::Status::Done) {
        std::cerr << "[MatchmakingService] ERROR: No se pudo conectar al puerto de admin del DedicatedServer ("
            << config.gameServerIp << ":" << config.gameServerDefaultPortTCPAdmin << std::endl;
        return false;
    }
    std::cout << "[MatchmakingService] Conectado al puerto de admin del DedicatedServer." << std::endl;
    dedicated_server_admin_socket.setBlocking(true); 
    return true;
}

void MatchmakingService::attemptReconnectToDedicatedServerAdmin() {
    std::cout << "[MatchmakingService] Intentando reconectar al puerto admin del DedicatedServer..." << std::endl;
    connectToDedicatedServerAdmin(); // Llama a la función de conexión
}

//Constructor.
MatchmakingService::MatchmakingService(const ServerConfig& server_config)
    : config(server_config) { 
    // Intenta conectar al iniciar el servicio
    if (!connectToDedicatedServerAdmin()) {
        std::cerr << "[MatchmakingService] Fallo inicial al conectar con el puerto admin del DS. Se reintentara mas tarde." << std::endl;
    }
}

MatchmakingService::~MatchmakingService() {
    if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any && dedicated_server_admin_socket.getRemoteAddress().has_value()) {
        dedicated_server_admin_socket.disconnect();
        std::cout << "[MatchmakingService] Desconectado del puerto admin del DedicatedServer." << std::endl;
    }
}

//Añade un cliente (que ya está LOGGED_IN) a la cola de matchmaking.
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

    bool added_to_actual_queue = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex); // Protege matchmakingQueue
        bool alreadyInQueue = false;
        // Comprueba si el socket ya está en la cola para evitar duplicados
        for (sf::TcpSocket* queuedSocket : matchmakingQueue) {
            if (queuedSocket == session.socket) {
                alreadyInQueue = true;
                break;
            }
        }

        if (!alreadyInQueue) {
            matchmakingQueue.push_back(session.socket);
            session.state = ClientState::IN_MATCHMAKING_QUEUE; 
            added_to_actual_queue = true;
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName()
                << " anadido a la lista de matchmaking. Total en cola: " << matchmakingQueue.size()
                << ". Estado del jugador: IN_MATCHMAKING_QUEUE" << std::endl;
        }
        else {
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName()
                << " ya estaba en la lista de matchmaking." << std::endl;
        }
    } // queueMutex se libera

    if (added_to_actual_queue) {
        sf::Packet response;
        response << PacketType::S_ADDED_TO_MATCHMAKING_QUEUE;
        if (tcpLayer.sendToClient(session.socket, response)) {
            std::cout << "[MatchmakingLogic] Notificacion S_ADDED_TO_MATCHMAKING_QUEUE enviada a "
                << session.playerInfo.getNickName() << std::endl;
        }
        else {
            std::cerr << "[MatchmakingLogic] Error enviando S_ADDED_TO_MATCHMAKING_QUEUE a "
                << session.playerInfo.getNickName() << std::endl;
            // Si falla el envío, revertir el estado y quitarlo de la cola
            session.state = ClientState::LOGGED_IN; 
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                matchmakingQueue.remove(session.socket); // Quitar de la cola
            }
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName()
                << " eliminado de la cola debido a error de envio. Estado revertido a LOGGED_IN." << std::endl;
        }
    }
}


//Se llama periódicamente (desde Server::matchmakingThreadLoop) para comprobar si hay suficientes jugadores en la matchmakingQueue para formar una partida.
void MatchmakingService::formMatches(std::unordered_map<sf::TcpSocket*, ClientSession>& clientSessionsMap, std::mutex& sessionsMutexRef, ServerTCP& tcpLayer) {
    std::lock_guard<std::mutex> lock(queueMutex); // Protege matchmakingQueue

    if (matchmakingQueue.size() < 2) {
       
        return;
    }

    std::cout << "[MatchmakingLogic::formMatches] Intentando formar partida. Jugadores en cola: " << matchmakingQueue.size() << std::endl;

    // Sacar los dos primeros jugadores de la cola
    sf::TcpSocket* socket1_ptr = matchmakingQueue.front();
    matchmakingQueue.pop_front();
    sf::TcpSocket* socket2_ptr = matchmakingQueue.front();
    matchmakingQueue.pop_front();

    ClientSession* session1 = nullptr;
    ClientSession* session2 = nullptr;

    // Acceder al mapa de sesiones para obtener los objetos ClientSession
   
    {
        std::lock_guard<std::mutex> sessionsLock(sessionsMutexRef);
        auto it1 = clientSessionsMap.find(socket1_ptr);
        if (it1 != clientSessionsMap.end()) {
            session1 = &it1->second;
        }
        else {
            std::cerr << "[MatchmakingLogic::formMatches] Error: No se encontro la sesion para socket1 ("
                << (socket1_ptr ? socket1_ptr->getRemoteAddress().value_or(sf::IpAddress::Any).toString() : "null")
                << ":" << (socket1_ptr ? socket1_ptr->getRemotePort() : 0) << ")." << std::endl;
        }
        auto it2 = clientSessionsMap.find(socket2_ptr);
        if (it2 != clientSessionsMap.end()) {
            session2 = &it2->second;
        }
        else {
            std::cerr << "[MatchmakingLogic::formMatches] Error: No se encontro la sesion para socket2 ("
                << (socket2_ptr ? socket2_ptr->getRemoteAddress().value_or(sf::IpAddress::Any).toString() : "null")
                << ":" << (socket2_ptr ? socket2_ptr->getRemotePort() : 0) << ")." << std::endl;
        }
    } // sessionsLock se libera aquí

    // Verificar si se encontraron ambas sesiones
    if (!session1 || !session2) {
        std::cerr << "[MatchmakingLogic::formMatches] Error: Sesion no encontrada para uno o ambos sockets. No se forma la partida." << std::endl;
      
        if (socket1_ptr && session1) matchmakingQueue.push_front(socket1_ptr);
        else if (socket1_ptr) { }

        if (socket2_ptr && session2) matchmakingQueue.push_front(socket2_ptr); 
        else if (socket2_ptr) {  }

        if (session1 && session2) {
           
        }
        return;
    }

    std::cout << "[MatchmakingLogic::formMatches] Jugador 1: " << session1->playerInfo.getNickName() << " (Estado: " << static_cast<int>(session1->state) << ")" << std::endl;
    std::cout << "[MatchmakingLogic::formMatches] Jugador 2: " << session2->playerInfo.getNickName() << " (Estado: " << static_cast<int>(session2->state) << ")" << std::endl;

    // Verificar que ambos jugadores estén en el estado correcto
    if (session1->state != ClientState::IN_MATCHMAKING_QUEUE || session2->state != ClientState::IN_MATCHMAKING_QUEUE) {
        std::cerr << "[MatchmakingLogic::formMatches] Error: Uno o ambos jugadores no estaban en estado IN_MATCHMAKING_QUEUE." << std::endl;
        // Devolver a la cola solo si el estado era IN_MATCHMAKING_QUEUE. Si no, cambiar a LOGGED_IN.
        if (session1->state == ClientState::IN_MATCHMAKING_QUEUE) {
            matchmakingQueue.push_front(socket1_ptr);
        }
        else {
           
            if (session1->state != ClientState::MATCHED) session1->state = ClientState::LOGGED_IN;
        }

        if (session2->state == ClientState::IN_MATCHMAKING_QUEUE) {
            matchmakingQueue.push_front(socket2_ptr);
        }
        else {
            if (session2->state != ClientState::MATCHED) session2->state = ClientState::LOGGED_IN;
        }
        return;
    }

    std::cout << "[MatchmakingLogic::formMatches] Emparejando " << session1->playerInfo.getNickName()
        << " con " << session2->playerInfo.getNickName() << std::endl;

    unsigned short client1_game_udp_port = assignNextGameClientUdpPort();
    unsigned short client2_game_udp_port = assignNextGameClientUdpPort();
    std::string new_room_id = generateRoomId();

    std::cout << "[MatchmakingLogic::formMatches] Info para DS: RoomID=" << new_room_id
        << ", P1_IP=" << session1->ipAddress.toString() << ", P1_UDP=" << client1_game_udp_port
        << ", P2_IP=" << session2->ipAddress.toString() << ", P2_UDP=" << client2_game_udp_port << std::endl;

    // Preparar paquete para notificar al Dedicated Server
    sf::Packet notification_to_dedicated;
    notification_to_dedicated << AdminPacketTypeDSSide::NOTIFY_NEW_GAME_DS; // Tipo de paquete (enum)
    notification_to_dedicated << new_room_id;                               // ID de la sala
    notification_to_dedicated << session1->ipAddress.toString();            // IP del Jugador 1
    notification_to_dedicated << client1_game_udp_port;                     // Puerto UDP asignado para Jugador 1
    notification_to_dedicated << session2->ipAddress.toString();            // IP del Jugador 2
    notification_to_dedicated << client2_game_udp_port;                     // Puerto UDP asignado para Jugador 2

    bool notified_dedicated_server = false;
    { // Bloque para el mutex del socket de admin
        std::lock_guard<std::mutex> admin_sock_lock(admin_socket_mutex);
        // Verificar si el socket de admin está conectado
        if (dedicated_server_admin_socket.getRemoteAddress() == sf::IpAddress::Any || !dedicated_server_admin_socket.getRemoteAddress().has_value()) {
            std::cout << "[MatchmakingLogic::formMatches] Socket de admin no conectado. Intentando reconectar..." << std::endl;
            if (!connectToDedicatedServerAdmin()) { // Intenta reconectar
                std::cerr << "[MatchmakingLogic::formMatches] Fallo al reconectar con el DS admin. No se puede notificar nueva partida." << std::endl;
                // Si falla la reconexión, devolver jugadores a la cola
                matchmakingQueue.push_front(socket2_ptr);
                matchmakingQueue.push_front(socket1_ptr);
                std::cout << "[MatchmakingLogic::formMatches] Jugadores devueltos a la cola (fallo reconexión DS)." << std::endl;
                return; // Salir de formMatches
            }
        }

        // Re-verificar si la conexión es válida después del intento de reconexión
        if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any && dedicated_server_admin_socket.getRemoteAddress().has_value()) {
            if (dedicated_server_admin_socket.send(notification_to_dedicated) == sf::Socket::Status::Done) {
                std::cout << "[MatchmakingLogic::formMatches] Notificacion enviada al DedicatedServer para la sala " << new_room_id << std::endl;
                notified_dedicated_server = true;
            }
            else {
                std::cerr << "[MatchmakingLogic::formMatches] ERROR: No se pudo enviar notificacion al DedicatedServer. Desconectando socket admin para intento futuro." << std::endl;
                dedicated_server_admin_socket.disconnect(); // Forzar reconexión la próxima vez
            }
        }
        else {
            std::cerr << "[MatchmakingLogic::formMatches] Socket de admin sigue sin estar conectado tras intento. No se puede notificar." << std::endl;
        }
    } // admin_sock_lock se libera aquí

    if (!notified_dedicated_server) {
        std::cerr << "[MatchmakingLogic::formMatches] Fallo al notificar al Dedicated Server. Devolviendo jugadores a la cola." << std::endl;
        matchmakingQueue.push_front(socket2_ptr);
        matchmakingQueue.push_front(socket1_ptr);
        std::cout << "[MatchmakingLogic::formMatches] Jugadores devueltos a la cola (fallo notificación DS)." << std::endl;
        return; // Salir de formMatches
    }

    // Si la notificación al DS fue exitosa, proceder a notificar a los clientes
    bool client1_is_player_one = true;  // Jugador 1 (session1) será P1
    bool client2_is_player_one = false; // Jugador 2 (session2) será P2

    // Paquete para el Jugador 1
    sf::Packet matchPacket1;
    matchPacket1 << PacketType::S_MATCH_FOUND;
    matchPacket1 << config.gameServerIp;          // IP del servidor de juego (Dedicated Server)
    matchPacket1 << config.gameServerDefaultPort; // Puerto UDP principal del Dedicated Server
    matchPacket1 << client1_game_udp_port;        // Puerto UDP específico que P1 debe usar para esta partida
    matchPacket1 << client1_is_player_one;       // Flag booleano indicando si es P1

    // Paquete para el Jugador 2
    sf::Packet matchPacket2;
    matchPacket2 << PacketType::S_MATCH_FOUND;
    matchPacket2 << config.gameServerIp;
    matchPacket2 << config.gameServerDefaultPort;
    matchPacket2 << client2_game_udp_port;
    matchPacket2 << client2_is_player_one;

    // Enviar los paquetes a los respectivos clientes
    bool sent1 = tcpLayer.sendToClient(session1->socket, matchPacket1);
    bool sent2 = tcpLayer.sendToClient(session2->socket, matchPacket2);

    if (sent1 && sent2) {
        // Si ambos envíos son exitosos, actualizar el estado de los jugadores
        session1->state = ClientState::MATCHED;
        session2->state = ClientState::MATCHED;
        std::cout << "[MatchmakingLogic::formMatches] Notificacion S_MATCH_FOUND enviada a ambos jugadores ("
            << session1->playerInfo.getNickName() << " y " << session2->playerInfo.getNickName()
            << "). Estados actualizados a MATCHED." << std::endl;
    }
    else {
        std::cerr << "[MatchmakingLogic::formMatches] Error enviando S_MATCH_FOUND a uno o ambos jugadores." << std::endl;

       

        std::cout << "[MatchmakingLogic::formMatches] Error en envio a clientes. Revirtiendo. Jugadores seran devueltos a la cola." << std::endl;

        // Revertir estado a IN_MATCHMAKING_QUEUE para que puedan ser emparejados de nuevo
        session1->state = ClientState::IN_MATCHMAKING_QUEUE;
        session2->state = ClientState::IN_MATCHMAKING_QUEUE;

        // Devolver a la cabeza de la cola
        matchmakingQueue.push_front(socket2_ptr);
        matchmakingQueue.push_front(socket1_ptr);

        if (!sent1) std::cerr << "Fallo al enviar S_MATCH_FOUND a " << session1->playerInfo.getNickName() << std::endl;
        if (!sent2) std::cerr << "Fallo al enviar S_MATCH_FOUND a " << session2->playerInfo.getNickName() << std::endl;

       
    }
}