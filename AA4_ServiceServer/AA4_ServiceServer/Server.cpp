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

//Bucle principal de ejecución del servidor de servicios. Este método se ejecuta en su propio std::thread.
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
        //// Pequeña pausa para no consumir 100% CPU si el update es muy rápido
        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    serverTCP.stopListener();
    std::cout << "[UnifiedServer] Listener TCP detingut." << std::endl;
}

// Inicia el proceso de detención del servidor
void Server::stop() {
    if (!server_running_flag.exchange(false)) { // Prevenir múltiples llamadas a stop
        if (!matchmaking_running_flag.load()) return; // Si matchmaking ya está detenido, salir
    }

    std::cout << "[UnifiedServer] Iniciant proces de parada..." << std::endl;
    
    matchmaking_running_flag = false;

    if (mmThread.joinable()) {
        std::cout << "[UnifiedServer] Esperant que el thread de matchmaking finalitzi..." << std::endl;
        mmThread.join();
        std::cout << "[UnifiedServer] Thread de matchmaking finalitzat." << std::endl;
    }

 

     {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        clientSessions.clear();
       
    }
    if (dbManager.isConnected) {
        dbManager.disconnectDB();
    }
    std::cout << "[UnifiedServer] Proces de parada completat." << std::endl;
}

//Callback que se ejecuta cuando ServerTCP acepta una nueva conexión de cliente.
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
        
      
        launcher.sendMapToClient(*session_ptr, serverTCP);
       
        session_ptr->state = ClientState::AWAITING_CREDENTIALS;
        std::cout << "[UnifiedServer] Client " << session_ptr->ipAddress.toString()
            << " canviat a AWAITING_CREDENTIALS." << std::endl;
    }
    else {
        std::cerr << "[UnifiedServer] Error critic: No s'ha pogut crear la sessio del client." << std::endl;
      
        clientSocket->disconnect();
      
    }
}

//Callback que se ejecuta cuando ServerTCP detecta que un cliente se ha desconectado.
void Server::handleClientDisconnected(sf::TcpSocket* clientSocket) {
    std::cout << "[UnifiedServer] Client desconnectat: " << clientSocket->getRemoteAddress().value_or(sf::IpAddress::Any).toString() << ":" << clientSocket->getRemotePort() << std::endl;
 

    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        clientSessions.erase(clientSocket); // Elimina la sesión del mapa.
    }
    
}


//Callback que se ejecuta cuando ServerTCP recibe un paquete de un cliente
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

// Bucle que se ejecuta en un hilo separado (mmThread) para periódicamente intentar formar partidas.
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