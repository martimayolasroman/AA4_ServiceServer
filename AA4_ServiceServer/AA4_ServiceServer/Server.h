#pragma once

#include<unordered_map>
#include "PlayerInfo.h"
#include "ServerTCP.h"
#include "DBManager.h"


#define SERVER "127.0.0.1:3306"
#define USERNAME "root"
#define PASSWORD "1234"
#define DATABASE "DuckGameDB"


class Server
{

public: 

	Server(unsigned short port);
	void run();

private:

	void processPacket(sf::TcpSocket* client, sf::Packet& packet);
	void handleDisconnection(sf::TcpSocket* client);

	ServerTCP serverTCP;
	DBManager dbManager;

	std::unordered_map<sf::TcpSocket*, PlayerInfo> connectedPlayers;//Map per asociar clients amb els seus nicknames
	void mostrarJugadorsConnectats();
	bool running = true;


	void ReceiveLogin(sf::Packet data, sf::TcpSocket* client);
	void ReceiveRegister(sf::Packet data, sf::TcpSocket* client);


	//void ReceiveCreateRoom(sf::Packet data, sf::TcpSocket* client);
	//void ReceiveJoinRoom(sf::Packet data, sf::TcpSocket* client);
	//void showPlayersInLobby(std::string& roomId);
	//void startGame(Room* room);


};

