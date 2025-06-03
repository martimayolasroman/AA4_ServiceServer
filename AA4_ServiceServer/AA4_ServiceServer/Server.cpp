#include "Server.h"






Server::Server(const ServerConfig& config): serverTCP(config.ServerPort),
dbManager(),						// Inicializa DBManager
serverConfig(config),
launcher(config.mapFilePath),        // Inicializa LauncherLogic
auth(dbManager),                     // Inicializa AuthLogic pasando dbManager
matchmaking(config),                 // Inicializa MatchmakingLogic
server_running_flag(false),
matchmaking_running_flag(false)
{
	// Connexió a la base de dades
	if (!dbManager.connectDataBase(config.dbHost, config.dbUser, config.dbPass, config.dbName)) {
		std::cout << "[SERVER] No s'ha pogut connectar a la base de dades." << std::endl;
	}
}

Server::~Server()
{
	stop();
	
}

void Server::run()
{
	if (!dbManager.isConnected) {
		std::cerr << "[UnifiedServer] No es pot iniciar, la BBDD no està connectada." << std::endl;
		return;
	}


	// 3. Registramos callbacks para el layer TCP
	serverTCP.setOnClientConnected([this](sf::TcpSocket* client) {
		this->handleClientConnected(client);
		});
	serverTCP.setOnClientDisconnected([this](sf::TcpSocket* client) {
		this->handleClientDisconnected(client);
		});
	serverTCP.setOnPacketReceived([this](sf::TcpSocket* client, sf::Packet& packet) {
		std::cout << "Callback packetreceived " << std::endl;
		this->processPacket(client, packet);
		});

	// 4. Iniciamos el listener TCP
	if (!serverTCP.startListener()) {
		std::cerr << "[UnifiedServer] No s'ha pogut iniciar el listener al port " << serverTCP.getPort() << std::endl;
		return;
	}
	server_running_flag = true;
	matchmaking_running_flag = true;

	// 5. Arrancamos el hilo de matchmaking
	mmThread = std::thread(&Server::matchmakingThreadLoop, this);

	std::cout << "[UnifiedServer] Iniciat al port " << serverTCP.getPort() << std::endl;


	
	// 6. Bucle principal : solo hace serverTCP.update()
	while (server_running_flag) {
		
		serverTCP.update();
		
	}

	// 7. Cuando se sale del bucle, paramos el listener TCP para no aceptar más clientes
	serverTCP.stopListener();
	std::cout << "[UnifiedServer] Listener TCP detingut." << std::endl;

	// 8. El join() del thread de matchmaking se hace en stop()

}

void Server::stop()
{
	if (!server_running_flag && !matchmaking_running_flag) { // Ya se está deteniendo o detenido
		return;
	}
	std::cout << "[UnifiedServer] Iniciant procés de parada..." << std::endl;
	server_running_flag = false; // Señal para el bucle principal
	matchmaking_running_flag = false; // Señal para el thread de matchmaking

	// Si el hilo de matchmaking sigue vivo, lo unimos
	if (mmThread.joinable()) {
		std::cout << "[UnifiedServer] Esperant que el thread de matchmaking finalitzi..." << std::endl;
		mmThread.join();
		std::cout << "[UnifiedServer] Thread de matchmaking finalitzat." << std::endl;
	}
	
	std::cout << "[UnifiedServer] Procés de parada completat." << std::endl;
}


// Cuando un cliente TCP se conecta, crea su sesión y le envía el mapa
void Server::handleClientConnected(sf::TcpSocket* clientSocket)
{

	std::cout << "[UnifiedServer] Client connectat: " << clientSocket->getRemoteAddress().value() << ":" << clientSocket->getRemotePort() << std::endl;
	ClientSession* session_ptr = nullptr;
	{
		// Bloqueamos el mapa de sesiones mientras insertamos la nueva
		std::lock_guard<std::mutex> lock(sessionsMutex);
		// Emplace devuelve un par (iterador, bool). Usamos el iterador para obtener la referencia.
		auto result = clientSessions.emplace(std::piecewise_construct,
			std::forward_as_tuple(clientSocket),
			std::forward_as_tuple(clientSocket));
		session_ptr = &result.first->second;
	}

	if (session_ptr) {
		// Fase de Launcher: Enviar mapa inmediatamente
		launcher.sendMapToClient(*session_ptr, serverTCP);
		// LauncherService internamente cambiará el estado de la sesión a AWAITING_CREDENTIALS
	}
	else {
		std::cerr << "[UnifiedServer] Error crític: No s'ha pogut crear la sessió del client." << std::endl;
		clientSocket->disconnect();
	}

}

// Cuando un cliente TCP se desconecta, lo eliminamos de nuestras estructuras
void Server::handleClientDisconnected(sf::TcpSocket* clientSocket)
{
	std::cout << "[UnifiedServer] Client desconnectat: " << clientSocket->getRemoteAddress().value() << ":" << clientSocket->getRemotePort() << std::endl;
	

	{
		std::lock_guard<std::mutex> lock(sessionsMutex);
		clientSessions.erase(clientSocket);
	}

	// Si el cliente estuviera en la cola de matchmaking, MatchmakingService se encarga
  // de limpiarlo en su próximo formMatches
}


// Procesa paquetes entrantes de clientes TCP según su estado
void Server::processPacket(sf::TcpSocket* client, sf::Packet& packet)
{

	std::cout << "Packet received from client" << std::endl;

	ClientSession* session_ptr = nullptr;
	{

		// Buscamos la sesión asociada al socket
		std::lock_guard<std::mutex> lock(sessionsMutex);
		auto it = clientSessions.find(client);
		if (it == clientSessions.end()) {
			std::cerr << "[UnifiedServer] Paquet rebut d'un client desconegut (sessió no trobada)." << std::endl;
			return;
		}
		session_ptr = &it->second;
	}


	PacketType type;
	 // Copia para extraer tipo sin afectar la original para los handlers
	packet >> type;


	switch (session_ptr->state) {

	case ClientState::AWAITING_CREDENTIALS:
		if (type == C_REQUEST_LOGIN || type == C_REQUEST_REGISTER) {
			// Pasamos el 'packet' original que contiene el tipo + datos. AuthLogic extraerá.
			auth.processAuthenticationRequest(type, packet, *session_ptr, serverTCP);
		}
		else {
			std::cerr << "[UnifiedServer] Paquet inesperat  per a l'estat AWAITING_CREDENTIALS de " << session_ptr->ipAddress.toString() << std::endl;
			
		}
		break;

	case ClientState::LOGGED_IN:
		if (type == C_REQUEST_MATCHMAKING_FRIENDLY) {
			
			matchmaking.addClientToQueue(*session_ptr, serverTCP);
		}
		else {
			std::cerr << "[UnifiedServer] Paquet inesperat  per a l'estat LOGGED_IN de " << session_ptr->playerInfo.getNickName() << std::endl;
		}
		break;

		

	default:
		std::cerr << "[UnifiedServer] Paquet rebut en un estat inesperat del client ("
			<< static_cast<int>(session_ptr->state) << ") Nick: " <<  session_ptr->playerInfo.getNickName() << std::endl;
			
		break;
	}


}



// Bucle que se ejecuta en hilo separado para buscar parejas en la cola
void Server::matchmakingThreadLoop()
{
	std::cout << "[UnifiedServer-MatchmakingThread] Iniciat." << std::endl;
	sf::Clock checkClock;
	// Usar serverConfig para el intervalo
	sf::Time checkInterval = sf::seconds(serverConfig.matchmakingCheckInterval);

	while (matchmaking_running_flag) {
		if (checkClock.getElapsedTime() >= checkInterval) {
			// Pasamos el mapa de sesiones y su mutex para que MatchmakingLogic pueda acceder a él
			// de forma segura si necesita actualizar el estado de los clientes emparejados.
			matchmaking.formMatches(clientSessions, sessionsMutex, serverTCP);
			checkClock.restart();
		}
		
	}
	std::cout << "[UnifiedServer-MatchmakingThread] Aturat." << std::endl;
}



