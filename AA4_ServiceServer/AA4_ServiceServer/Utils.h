#pragma once
#include <iostream>
#include <SFML/Network.hpp>


// Define todos los tipos de paquete usados tanto cliente-servidor como servidor-cliente
enum  PacketType
{
    // Cliente a Servidor
    C_REQUEST_LOGIN = 1,
    C_REQUEST_REGISTER = 2,
    C_REQUEST_MATCHMAKING_FRIENDLY = 3,
    C_MAP_RECEIVED_ACK = 4, 

    // Servidor a Cliente
    S_MAP_DATA = 100,
    S_LOGIN_OK = 101,
    S_LOGIN_FAIL = 102,
    S_REGISTER_OK = 103,
    S_REGISTER_FAIL = 104,
    S_ADDED_TO_MATCHMAKING_QUEUE = 105,
    S_MATCH_FOUND = 106, 
    S_ERROR_GENERAL = 107, // Para errores genéricos

   //Servidor(s) a servidor(d)
    NOTIFY_NEW_GAME= 200,

    UNKNOWN = 255
};


// Operators para leer / escribir PacketType directamente en sf::Packet

inline sf::Packet& operator >> (sf::Packet& packet, PacketType& tipo) {

	int temp;
	packet >> temp;
	tipo = static_cast<PacketType>(temp);

	return packet;
}

inline sf::Packet& operator << (sf::Packet& packet, PacketType tipo) {
	packet << static_cast<int>(tipo);
	return packet;
}
