#include "MatchmakingService.h"




// Contador para asignar puertos UDP a los clientes para el juego.
static unsigned short nextGameClientUdpPort = 57000; // Puerto inicial de ejemplo
static std::mutex port_assignment_mutex;

unsigned short assignNextGameClientUdpPort() {
    std::lock_guard<std::mutex> lock(port_assignment_mutex);
    unsigned short assigned_port = nextGameClientUdpPort;
    nextGameClientUdpPort++;
    if (nextGameClientUdpPort > 57100) { // Limitar el rango para el ejemplo
        nextGameClientUdpPort = 57000; // Reiniciar (esto es muy básico)
    }
    return assigned_port;
}

std::string generateRoomId() {
    // Generador simple de ID de sala
    static int room_counter = 0;
    std::ostringstream oss;
    oss << "room_" << room_counter++;
    return oss.str();
}





MatchmakingService::MatchmakingService(const ServerConfig& server_config) : config(server_config)
{

    if (dedicated_server_admin_socket.connect(sf::IpAddress::resolve(config.gameServerIp).value(), config.gameServerDefaultPortTCPAdmin) != sf::Socket::Status::Done) {
        std::cerr << "[MatchmakingService] ERROR: No se pudo conectar al puerto de admin del DedicatedServer ("
            << config.gameServerIp << ":" << config.gameServerDefaultPortTCPAdmin << ")" << std::endl;
        // Marcar un estado de error o reintentar más tarde.
    }
    else {
        std::cout << "[MatchmakingService] Conectado al puerto de admin del DedicatedServer." << std::endl;
        dedicated_server_admin_socket.setBlocking(true); // Para envíos síncronos de notificaciones
    }

}

void MatchmakingService::addClientToQueue(ClientSession& session, ServerTCP& tcpLayer)
{
    std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName()
        << " (" << session.ipAddress.toString() << ":" << session.port
        << ") solicita matchmaking amistós." << std::endl;

    bool added = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        bool alreadyInQueue = false;
        for (sf::TcpSocket* queuedSocket : matchmakingQueue) {
            if (queuedSocket == session.socket) {
                alreadyInQueue = true;
                break;
            }
        }

        if (!alreadyInQueue) {
            matchmakingQueue.push_back(session.socket);
            session.state = ClientState::IN_MATCHMAKING_QUEUE;
            added = true;
        }
        else {
            std::cout << "[MatchmakingLogic] Jugador " << session.playerInfo.getNickName() << " ja estaba a la cua." << std::endl;
        }
    }

    if (added) {
        sf::Packet response;
        response << PacketType::S_ADDED_TO_MATCHMAKING_QUEUE;
        tcpLayer.sendToClient(session.socket, response);
    }
}


void MatchmakingService::formMatches(std::unordered_map<sf::TcpSocket*, ClientSession>& clientSessionsMap, std::mutex& sessionsMutexRef, ServerTCP& tcpLayer)
{
    std::lock_guard<std::mutex> lock(queueMutex); // Bloquea la cola primero

    if (matchmakingQueue.size() >= 2) {
        sf::TcpSocket* socket1_ptr = matchmakingQueue.front();
        matchmakingQueue.pop_front();
        sf::TcpSocket* socket2_ptr = matchmakingQueue.front();
        matchmakingQueue.pop_front();

        ClientSession* session1 = nullptr;
        ClientSession* session2 = nullptr;

        { // Alcance para el bloqueo del mapa de sesiones
            std::lock_guard<std::mutex> sessionsLock(sessionsMutexRef);
            auto it1 = clientSessionsMap.find(socket1_ptr);
            if (it1 != clientSessionsMap.end()) {
                session1 = &it1->second;
            }
            auto it2 = clientSessionsMap.find(socket2_ptr);
            if (it2 != clientSessionsMap.end()) {
                session2 = &it2->second;
            }
        }


        if (session1 && session2) {
            // Asegurarse de que los clientes todavía están en el estado correcto (IN_MATCHMAKING_QUEUE)
            // Esto es una doble comprobación, ya que deberían estarlo si estaban en la cola.
            if (session1->state != ClientState::IN_MATCHMAKING_QUEUE || session2->state != ClientState::IN_MATCHMAKING_QUEUE) {
                std::cerr << "[MatchmakingLogic] Error: Un jugador emparellat no estava en estat IN_MATCHMAKING_QUEUE." << std::endl;
                // Devolver a la cola si uno está bien y el otro no, o descartar.
                // Por simplicidad, los descartamos. Podrías volver a añadirlos al principio de la cola.
                if (session1->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket1_ptr);
                if (session2->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket2_ptr);
                return;
            }


            std::cout << "[MatchmakingLogic] Emparellant " << session1->playerInfo.getNickName()
                << " amb " << session2->playerInfo.getNickName() << std::endl;


            // 1. Asignar puertos UDP para el juego a cada cliente
            unsigned short client1_game_udp_port = assignNextGameClientUdpPort();
            unsigned short client2_game_udp_port = assignNextGameClientUdpPort();
            std::string new_room_id = generateRoomId();

            // 2. Preparar y enviar notificación al DedicatedServer
            sf::Packet notification_to_dedicated;


            notification_to_dedicated << NOTIFY_NEW_GAME; // Asegúrate que el tipo coincida con el servidor dedicado
            notification_to_dedicated << new_room_id
                << session1->ipAddress.toString() // IP del cliente 1 (obtenida de su conexión TCP)
                << client1_game_udp_port          // Puerto UDP que cliente 1 usará
                << session2->ipAddress.toString() // IP del cliente 2
                << client2_game_udp_port;         // Puerto UDP que cliente 2 usará

            bool notified_dedicated_server = false;


            if (dedicated_server_admin_socket.getRemoteAddress() != sf::IpAddress::Any) { // Comprueba si está conectada
                if (dedicated_server_admin_socket.send(notification_to_dedicated) == sf::Socket::Status::Done) {
                    std::cout << "[MatchmakingService] Notificación enviada al DedicatedServer para la sala " << new_room_id << std::endl;
                    notified_dedicated_server = true;
                }
                else {
                    std::cerr << "[MatchmakingService] ERROR: No se pudo enviar notificación al DedicatedServer." << std::endl;
                    // Reintentar conexión al admin del dedicated server o marcar error.
                    dedicated_server_admin_socket.disconnect(); // Intentar reconectar la próxima vez.
                }
            }



            // 3. Enviar información a los clientes
            sf::Packet matchPacket1;
            matchPacket1 << S_MATCH_FOUND // Usa tu enum global
                << config.gameServerIp         // IP del DedicatedServer
                << config.gameServerDefaultPort // Puerto UDP principal del DedicatedServer
                << client1_game_udp_port;     // Puerto UDP local que cliente 1 debe usar

            sf::Packet matchPacket2;
            matchPacket2 << PacketType::S_MATCH_FOUND // Usa tu enum global
                << config.gameServerIp
                << config.gameServerDefaultPort
                << client2_game_udp_port;     // Puerto UDP local que cliente 2 debe usar





            /*sf::Packet matchPacket;
            matchPacket << S_MATCH_FOUND;
            matchPacket << config.gameServerIp << config.gameServerDefaultPort;*/
            

            // Enviar a ambos clientes
            bool sent1 = tcpLayer.sendToClient(session1->socket, matchPacket1);
            // Es importante crear un nuevo paquete o limpiar el anterior si SFML Packet no lo hace automáticamente al enviar.
            // En este caso, el mismo paquete se envía a ambos, lo cual está bien.
            bool sent2 = tcpLayer.sendToClient(session2->socket, matchPacket2);

            if (sent1 && sent2) {
                session1->state = ClientState::MATCHED;
                session2->state = ClientState::MATCHED;
                std::cout << "[MatchmakingLogic] Notificació de partida enviada a ambdós jugadors." << std::endl;
            }
            else {
                std::cerr << "[MatchmakingLogic] Error enviant notificació de partida a un o ambdós jugadors." << std::endl;
                // ¿Qué hacer aquí? ¿Intentar volver a ponerlos en cola?
                // Si el envío falla, es probable que el socket esté roto.
                // ServerTCP debería manejar la desconexión.
                // Si un envío falla pero el otro no, el jugador que recibió podría quedar esperando.
                // Es una situación delicada. Por ahora, si falla, la sesión se eliminará cuando se detecte la desconexión.
                if (!sent1 && session1->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket1_ptr); // Reintentar para s1
                if (!sent2 && session2->state == ClientState::IN_MATCHMAKING_QUEUE) matchmakingQueue.push_front(socket2_ptr); // Reintentar para s2
            }
        }
        else {
            std::cerr << "[MatchmakingLogic] Error: No s'ha trobat la sessió per a un o ambdós sockets de la cua." << std::endl;
            // Si la sesión no existe pero el socket estaba en la cola, es un estado inconsistente.
            // Simplemente no los volvemos a añadir.
            if (socket1_ptr && !session1) std::cout << "Socket 1 (" << socket1_ptr << ") no tenia sessió." << std::endl;
            if (socket2_ptr && !session2) std::cout << "Socket 2 (" << socket2_ptr << ") no tenia sessió." << std::endl;
        }
    }
}
