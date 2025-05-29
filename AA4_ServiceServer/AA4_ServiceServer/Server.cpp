#include "Server.h"






Server::Server(const ServerConfig& config): serverTCP(config.ServerPort),
dbManager(), // Se inicializa por defecto, luego se conecta
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
		
	}

	// Detener el listener antes de esperar a los threads para evitar nuevas conexiones/paquetes
	serverTCP.stopListener();
	std::cout << "[UnifiedServer] Listener TCP detingut." << std::endl;

	// La detención del thread de matchmaking se maneja en stop()

}

void Server::stop()
{
	if (!server_running_flag && !matchmaking_running_flag) { // Ya se está deteniendo o detenido
		return;
	}
	std::cout << "[UnifiedServer] Iniciant procés de parada..." << std::endl;
	server_running_flag = false; // Señal para el bucle principal
	matchmaking_running_flag = false; // Señal para el thread de matchmaking

	if (mmThread.joinable()) {
		std::cout << "[UnifiedServer] Esperant que el thread de matchmaking finalitzi..." << std::endl;
		mmThread.join();
		std::cout << "[UnifiedServer] Thread de matchmaking finalitzat." << std::endl;
	}
	
	std::cout << "[UnifiedServer] Procés de parada completat." << std::endl;
}

void Server::handleClientConnected(sf::TcpSocket* clientSocket)
{

	std::cout << "[UnifiedServer] Client connectat: " << clientSocket->getRemoteAddress().value() << ":" << clientSocket->getRemotePort() << std::endl;
	ClientSession* session_ptr = nullptr;
	{
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
		// El estado del cliente se actualiza dentro de sendMapToClient
	}
	else {
		std::cerr << "[UnifiedServer] Error crític: No s'ha pogut crear la sessió del client." << std::endl;
		clientSocket->disconnect();
	}

}

void Server::handleClientDisconnected(sf::TcpSocket* clientSocket)
{
	std::cout << "[UnifiedServer] Client desconnectat: " << clientSocket->getRemoteAddress().value() << ":" << clientSocket->getRemotePort() << std::endl;
	// Eliminar de la cola de matchmaking si estaba allí
	// (MatchmakingLogic ahora gestiona su propia cola, así que esta lógica específica se mueve allí
	// o se llama a un método de MatchmakingLogic para limpiar)
	// Para simplificar, y dado que MatchmakingLogic::formMatches itera sobre su propia cola,
	// solo necesitamos eliminar la sesión del servidor.

	// Si MatchmakingLogic necesitara ser notificada, harías algo como:
	// matchmaking.handleClientDisconnected(clientSocket);

	{
		std::lock_guard<std::mutex> lock(sessionsMutex);
		clientSessions.erase(clientSocket);
	}
}

void Server::processPacket(sf::TcpSocket* client, sf::Packet& packet)
{

	std::cout << "Packet received from client" << std::endl;

	ClientSession* session_ptr = nullptr;
	{
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
			// Enviar error al cliente? Desconectarlo?
		}
		break;

	case ClientState::LOGGED_IN:
		if (type == C_REQUEST_MATCHMAKING_FRIENDLY) {
			// El paquete para C_REQUEST_MATCHMAKING_FRIENDLY podría no tener más datos después del tipo.
			matchmaking.addClientToQueue(*session_ptr, serverTCP);
		}
		else {
			std::cerr << "[UnifiedServer] Paquet inesperat  per a l'estat LOGGED_IN de " << session_ptr->playerInfo.getNickName() << std::endl;
		}
		break;

		// case ClientState::AWAITING_MAP_ACK:
		//     // ...
		//     break;

	default:
		std::cerr << "[UnifiedServer] Paquet rebut en un estat inesperat del client ("
			<< static_cast<int>(session_ptr->state) << ") Nick: " << (session_ptr->playerInfo.getNickName().empty() ? "N/A" : session_ptr->playerInfo.getNickName())
			<< std::endl;
		break;
	}


}

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
		//std::this_thread::sleep_for(sf::milliseconds(200)); // Evitar uso excesivo de CPU
	}
	std::cout << "[UnifiedServer-MatchmakingThread] Aturat." << std::endl;
}

void Server::checkQueueAndFormMatches()
{

}

