#pragma once


#include <SFML/Network.hpp>
#include "PlayerInfo.h"


enum class ClientState {
    CONNECTED,              // Recién conectado, esperando envío de mapa
    AWAITING_MAP_ACK,       // Mapa enviado, esperando ACK del cliente (opcional)
    AWAITING_CREDENTIALS,   // Listo para Login/Register
    LOGGED_IN,              // Autenticado, listo para Matchmaking
    IN_MATCHMAKING_QUEUE,   // Buscando partida
    MATCHED                 // Emparejado, esperando info del Game Server (se desconectará de este servidor)
    // Podrías añadir más estados si es necesario
};

class ClientSession
{

public:
    sf::TcpSocket* socket;
    PlayerInfo playerInfo; // Puede llenarse después del login
    ClientState state;
    sf::IpAddress ipAddress;
    unsigned short port;

    ClientSession(sf::TcpSocket* clientSocket)
        : socket(clientSocket), state(ClientState::CONNECTED),
        ipAddress(clientSocket->getRemoteAddress().value()),
        port(clientSocket->getRemotePort()) {}

  


};

