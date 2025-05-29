#pragma once

#pragma once
#include "ServerTCP.h"      
#include "DBManager.h"
#include "ClientSession.h"
#include "Utils.h"
#include "ServerConfig.h"        
#include <unordered_map>
#include <list>
#include <atomic>
#include <thread>
#include <mutex> // Para proteger la cola de matchmaking
#include <fstream> // Para leer el mapa
#include <vector>  // Para leer el mapa
#include "LauncherService.h"
#include "MatchmakingService.h"
#include "AuthService.h"

class Server
{

public: 

	Server(const ServerConfig& config);
	~Server();

	void run();
	void stop();

private:

	void handleClientConnected(sf::TcpSocket* clientSocket);
	void handleClientDisconnected(sf::TcpSocket* clientSocket);
	void processPacket(sf::TcpSocket* client, sf::Packet& packet);


	

	// Matchmaking
	void matchmakingThreadLoop(); // Bucle para el thread de matchmaking
	void checkQueueAndFormMatches(); // Lógica real de emparejamiento

	

	ServerTCP serverTCP;
	DBManager dbManager;
	ServerConfig serverConfig;

	// Módulos de Lógica
	LauncherService launcher;
	AuthService auth;
	MatchmakingService matchmaking;

	std::unordered_map<sf::TcpSocket*, ClientSession> clientSessions;
	std::mutex sessionsMutex; // Para proteger clientSessions


	// Matchmaking Queue
	std::list<sf::TcpSocket*> matchmakingQueue; // Almacena sockets de clientes en cola
	std::mutex matchmakingMutex; // Para proteger matchmakingQueue
	std::thread mmThread; // Thread para el matchmaking

	std::atomic<bool> matchmaking_running_flag;
	std::atomic<bool> server_running_flag;





};

