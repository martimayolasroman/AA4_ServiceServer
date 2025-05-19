#include "Server.h"


enum  PacketType
{
	HANDSHAKE = 0,
	LOGIN,
	REGISTER,
	CREATE_ROOM,
	JOIN_ROOM,
	DISCONNECT,
	LOGIN_OK,
	LOGIN_FAIL,
	REGISTER_OK,
	REGISTER_FAIL,
	START_GAME,
	UNKNOWN
};


sf::Packet& operator >> (sf::Packet& packet, PacketType& tipo) {

	int temp;
	packet >> temp;
	tipo = static_cast<PacketType>(temp);

	return packet;
}


Server::Server(unsigned short port): serverTCP(port)
{
	// Connexió a la base de dades
	if (!dbManager.connectDataBase(SERVER, USERNAME, PASSWORD, DATABASE)) {
		std::cout << "[SERVER] No s'ha pogut connectar a la base de dades." << std::endl;
	}
}

void Server::run()
{
	if (!serverTCP.start()) {
		std::cerr << "Error al iniciar el servidor " << std::endl;
		return;
	}

	while (running)
	{
		serverTCP.update();

		std::vector<sf::TcpSocket*> clients = serverTCP.getConnectedClients();

		for (int i = 0; i < clients.size(); i++) {
			sf::TcpSocket* client = clients[i];

			sf::Packet packet;
			if (client->receive(packet) == sf::Socket::Status::Done) {
				processPacket(client, packet);
			}
		}






	}

	serverTCP.stop();

}

void Server::processPacket(sf::TcpSocket* client, sf::Packet& packet)
{
	PacketType tipo;
	packet >> tipo;

	switch (tipo)
	{
	case HANDSHAKE:
		break;
	case LOGIN:
		ReceiveLogin(packet, client);
		break;
	case REGISTER:
		std::cerr << " --PROCESS PACKET(REGISTER) -- Mensaje recibido  " << std::endl;
		ReceiveRegister(packet, client);
		break;
	case DISCONNECT:
		break;
	case UNKNOWN:
		break;
	default:
		break;
	}


}

void Server::handleDisconnection(sf::TcpSocket* client)
{
	connectedPlayers.erase(client);
	std::cout << "Jugador desconnectat." << std::endl;
}

void Server::mostrarJugadorsConnectats()
{
	std::cout << "[SERVER] -- Jugadors conectats --" << std::endl;

	if (connectedPlayers.empty()) {
		std::cout << "Cap jugador conectat" << std::endl;
		return;
	}

	for (auto it = connectedPlayers.begin(); it != connectedPlayers.end(); ++it) {
		const PlayerInfo& p = it->second;

		std::cout << "Nickname: " << p.getNickName()
			<< "|IP: " << p.getIp().toString()
			<< "|Port: " << p.getPort()
			<< "|Connexió: " << (p.isConnected() ? "Sí" : "No") << std::endl;

	}




}

void Server::ReceiveLogin(sf::Packet data, sf::TcpSocket* client)
{




	std::string nickname;
	std::string password;
	data >> nickname >> password;


	std::cerr << " --LOGIN -- Mensaje recibido : " << nickname << ", " << password << std::endl;

	// REGISTRAR A BASE DE DADES

	if (dbManager.validateUser(nickname, password)) {
		PlayerInfo player(nickname, client->getRemoteAddress().value(), client->getRemotePort());
		connectedPlayers[client] = player;

		sf::Packet packet;
		packet << LOGIN_OK;


		serverTCP.sendToClient(client, packet);
	}
	else {

		sf::Packet packet;
		packet << LOGIN_FAIL;
		serverTCP.sendToClient(client, packet);

	}




	mostrarJugadorsConnectats();

}

void Server::ReceiveRegister(sf::Packet data, sf::TcpSocket* client)
{
	std::string nickname;
	std::string password;
	data >> nickname >> password;

	std::cerr << " --REGISTER -- Mensaje recibido : " << nickname << ", " << password << std::endl;

	// VEURE SI EXISTEIX A LA BASE DE DADES

	if (dbManager.userExist(nickname)) {
		std::cerr << " --REGISTER DB-- USUARIA JA EXISTEIX A LA BD  " << std::endl;

		sf::Packet packet;
		packet << REGISTER_FAIL;
		serverTCP.sendToClient(client, packet);

	}
	else {
		bool ok = dbManager.registerUser(nickname, password);

		if (ok) {

			PlayerInfo player(nickname, client->getRemoteAddress().value(), client->getRemotePort());
			connectedPlayers[client] = player;

			sf::Packet packet;
			packet << REGISTER_OK;
			serverTCP.sendToClient(client, packet);

		}

	}


}