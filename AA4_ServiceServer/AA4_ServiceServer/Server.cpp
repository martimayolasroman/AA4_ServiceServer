#include "Server.h"
#include <iostream>

Server::Server(const ServerConfig& config) :
    serverTCP(config.ServerPort),
    dbManager(),
    serverConfig(config),
    launcher(config.mapFilePath),  
    auth(dbManager),
    matchmaking(config),           
    server_running_flag(false),
    matchmaking_running_flag(false) {
    if (!dbManager.connectDataBase(config.dbHost, config.dbUser, config.dbPass, config.dbName)) {
        std::cerr << "[SERVER] No s'ha pogut connectar a la base de dades. El servidor podria no funcionar correctament." << std::endl;
     }
}

Server::~Server() {
    stop();
}

void Server::run() {
    if (!dbManager.isConnected) {
        std::cerr << "[UnifiedServer] No es pot iniciar, la BBDD no esta connectada." << std::endl;
        return;
    }

    serverTCP.setOnClientConnected([this](sf::TcpSocket* client) {
        this->handleClientConnected(client);
        });
    serverTCP.setOnClientDisconnected([this](sf::TcpSocket* client) {
        this->handleClientDisconnected(client);
        });
    serverTCP.setOnPacketReceived([this](sf::TcpSocket* client, sf::Packet& packet) {
        this->processPacket(client, packet);
        });

    if (!serverTCP.startListener()) {
        std::cerr << "[UnifiedServer] No s'ha pogut iniciar el listener al port " << serverTCP.getPort() << std::endl;
        return;
    }
    server_running_flag = true;
    matchmaking_running_flag = true;

    mmThread = std::thread(&Server::matchmakingThreadLoop, this);
    std::cout << "[UnifiedServer] Iniciat al port " << serverTCP.getPort() << std::endl;

    while (server_running_flag) {
        serverTCP.update();
        // Pequeña pausa para no consumir 100% CPU si el update es muy rápido
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    serverTCP.stopListener();
    std::cout << "[UnifiedServer] Listener TCP detingut." << std::endl;
}

void Server::stop() {
    if (!server_running_flag.exchange(false)) { // Prevenir múltiples llamadas a stop
        if (!matchmaking_running_flag.load()) return; // Si matchmaking ya está detenido, salir
    }

    std::cout << "[UnifiedServer] Iniciant proces de parada..." << std::endl;
    // server_running_flag ya está en false.
    matchmaking_running_flag = false;

    if (mmThread.joinable()) {
        std::cout << "[UnifiedServer] Esperant que el thread de matchmaking finalitzi..." << std::endl;
        mmThread.join();
        std::cout << "[UnifiedServer] Thread de matchmaking finalitzat." << std::endl;
    }

 

     {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        clientSessions.clear(); // Los ClientSession se destruirán, pero los sf::TcpSocket* no se borran aquí
        // ServerTCP debería ser el dueño de los sockets y borrarlos.
    }
    if (dbManager.isConnected) {
        dbManager.disconnectDB();
    }
    std::cout << "[UnifiedServer] Proces de parada completat." << std::endl;
}

void Server::handleClientConnected(sf::TcpSocket* clientSocket) {
    std::cout << "[UnifiedServer] Client connectat: " << clientSocket->getRemoteAddress().value_or(sf::IpAddress::Any).toString() << ":" << clientSocket->getRemotePort() << std::endl;
    ClientSession* session_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto result = clientSessions.emplace(std::piecewise_construct,
            std::forward_as_tuple(clientSocket),
            std::forward_as_tuple(clientSocket));
        session_ptr = &result.first->second;
    }

    if (session_ptr) {
        
       // std::cout << "[MatchmakingLogic] Enviando mapa a " << session.playerInfo.getNickName() << std::endl;
        launcher.sendMapToClient(*session_ptr, serverTCP);
        // El cliente ahora está en ClientState::CONNECTED por defecto.
        // Cambiamos a AWAITING_CREDENTIALS directamente. No se envía mapa aquí.
        session_ptr->state = ClientState::AWAITING_CREDENTIALS;
        std::cout << "[UnifiedServer] Client " << session_ptr->ipAddress.toString()
            << " canviat a AWAITING_CREDENTIALS." << std::endl;
    }
    else {
        std::cerr << "[UnifiedServer] Error critic: No s'ha pogut crear la sessio del client." << std::endl;
        // ServerTCP debería manejar la desconexión y borrado del socket si la creación falla aquí
        // o si nosotros mismos lo desconectamos.
        clientSocket->disconnect();
        // No borramos clientSocket aquí, ServerTCP lo hará en su limpieza si está en su lista.
    }
}

void Server::handleClientDisconnected(sf::TcpSocket* clientSocket) {
    std::cout << "[UnifiedServer] Client desconnectat: " << clientSocket->getRemoteAddress().value_or(sf::IpAddress::Any).toString() << ":" << clientSocket->getRemotePort() << std::endl;
 

    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        clientSessions.erase(clientSocket); // Elimina la sesión del mapa.
    }
    // El sf::TcpSocket* es borrado por ServerTCP cuando lo elimina de su lista de clientes.
}

void Server::processPacket(sf::TcpSocket* client, sf::Packet& packet) {
    ClientSession* session_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = clientSessions.find(client);
        if (it == clientSessions.end()) {
            std::cerr << "[UnifiedServer] Paquet rebut d'un client desconegut (sessio no trobada)." << std::endl;
            return;
        }
        session_ptr = &it->second;
    }

    PacketType type;
 
    if (!(packet >> type)) { // Extraer tipo del paquete original
        std::cerr << "[UnifiedServer] Error al extraer PacketType del paquete de " << session_ptr->ipAddress.toString() << std::endl;
        return;
    }


    // std::cout << "[UnifiedServer] Paquet rebut de " << session_ptr->ipAddress.toString()
    //           << " Tipus: " << static_cast<int>(type)
    //           << " Estat actual del client: " << static_cast<int>(session_ptr->state) << std::endl;

    switch (session_ptr->state) {
    case ClientState::AWAITING_CREDENTIALS:
        if (type == C_REQUEST_LOGIN || type == C_REQUEST_REGISTER) {
 
            auth.processAuthenticationRequest(type, packet, *session_ptr, serverTCP);
        }
        else {
            std::cerr << "[UnifiedServer] Paquet inesperat (" << static_cast<int>(type)
                << ") per a l'estat AWAITING_CREDENTIALS de " << session_ptr->ipAddress.toString() << std::endl;
        }
        break;

    case ClientState::LOGGED_IN:
        if (type == C_REQUEST_MATCHMAKING_FRIENDLY) {
            // MatchmakingService se encargará de todo. No necesita más datos del paquete aquí.
            matchmaking.addClientToQueue(*session_ptr, serverTCP);
        }
        else {
            std::cerr << "[UnifiedServer] Paquet inesperat (" << static_cast<int>(type)
                << ") per a l'estat LOGGED_IN de " << session_ptr->playerInfo.getNickName() << std::endl;
        }
        break;

    case ClientState::IN_MATCHMAKING_QUEUE:
    case ClientState::MATCHED:
        std::cout << "[UnifiedServer] Paquet rebut (" << static_cast<int>(type)
            << ") de " << session_ptr->playerInfo.getNickName()
            << " que esta en estat " << static_cast<int>(session_ptr->state)
            << ". No s'esperen paquets TCP en aquest estat." << std::endl;
        // No se esperan más paquetes TCP del cliente una vez está en cola o emparejado (para este servidor)
        break;

    default:
        std::cerr << "[UnifiedServer] Paquet rebut (" << static_cast<int>(type)
            << ") en un estat inesperat del client ("
            << static_cast<int>(session_ptr->state) << ") Nick: "
            << (session_ptr->playerInfo.getNickName().empty() ? "N/A" : session_ptr->playerInfo.getNickName())
            << std::endl;
        break;
    }
}

void Server::matchmakingThreadLoop() {
    std::cout << "[UnifiedServer-MatchmakingThread] Iniciat." << std::endl;
    sf::Clock checkClock;
    sf::Time checkInterval = sf::seconds(serverConfig.matchmakingCheckInterval);

    while (matchmaking_running_flag) {
        if (checkClock.getElapsedTime() >= checkInterval) {
            matchmaking.formMatches(clientSessions, sessionsMutex, serverTCP);
            checkClock.restart();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Evitar uso excesivo de CPU
    }
    std::cout << "[UnifiedServer-MatchmakingThread] Aturat." << std::endl;
}