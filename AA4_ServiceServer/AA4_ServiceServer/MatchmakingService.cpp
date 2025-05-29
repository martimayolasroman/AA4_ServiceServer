#include "MatchmakingService.h"




MatchmakingService::MatchmakingService(const ServerConfig& server_config) : config(server_config)
{

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


            std::cout << "[MatchmakingLogic] Emparejant " << session1->playerInfo.getNickName()
                << " amb " << session2->playerInfo.getNickName() << std::endl;

            sf::Packet matchPacket;
            matchPacket << PacketType::S_MATCH_FOUND;
            matchPacket << config.gameServerIp << config.gameServerDefaultPort;
            // Podrías añadir ID de sala único si el gameserver lo necesita, o los nicks.
            // matchPacket << generateUniqueRoomId(); 

            // Enviar a ambos clientes
            bool sent1 = tcpLayer.sendToClient(session1->socket, matchPacket);
            // Es importante crear un nuevo paquete o limpiar el anterior si SFML Packet no lo hace automáticamente al enviar.
            // En este caso, el mismo paquete se envía a ambos, lo cual está bien.
            bool sent2 = tcpLayer.sendToClient(session2->socket, matchPacket);

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
