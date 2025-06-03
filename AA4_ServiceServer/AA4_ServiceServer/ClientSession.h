#pragma once


#include <SFML/Network.hpp>
#include "PlayerInfo.h"

// Estados en los que puede estar un cliente en el ciclo de vida
enum class ClientState {
    CONNECTED,              // Recién conectado, esperando envío de mapa
    AWAITING_MAP_ACK,       // Mapa enviado, esperando ACK del cliente
    AWAITING_CREDENTIALS,   // Listo para Login/Register
    LOGGED_IN,              // Autenticado, listo para Matchmaking
    IN_MATCHMAKING_QUEUE,   // Buscando partida
    MATCHED                 // Emparejado, esperando info del Game Server (se desconectará de este servidor)
    
};

class ClientSession
{

public:
    sf::TcpSocket* socket;      // Socket TCP asociado a este cliente
    PlayerInfo playerInfo;      // Contiene nickname, IP, puerto y estado conectado
    ClientState state;           // Estado actual en el ciclo de vida
    sf::IpAddress ipAddress;    // IP remota
    unsigned short port;         // Puerto remoto


    // Constructor: asigna el socket y extrae IP y puerto
    ClientSession(sf::TcpSocket* clientSocket)
        : socket(clientSocket), state(ClientState::CONNECTED),
        ipAddress(clientSocket->getRemoteAddress().value()),
        port(clientSocket->getRemotePort()) {}

  


};

