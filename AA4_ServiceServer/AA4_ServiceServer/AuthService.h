#pragma once

#include <string>
#include "ClientSession.h"
#include "Utils.h"
#include "ServerTCP.h"
#include "DBManager.h" 


class AuthService
{

public:

    AuthService(DBManager& databaseManager);
   

    // Procesa la petición de login o registro.
      // 'packet_type' es el tipo ya extraído (C_REQUEST_LOGIN o C_REQUEST_REGISTER).
      // 'packet_data' es el paquete SFML del cual extraer nickname y password.
      // Actualiza session.state y session.playerInfo en caso de éxito.
      // Envía la respuesta al cliente usando tcpLayer.
    void processAuthenticationRequest(PacketType packet_type, sf::Packet& packet_data,
        ClientSession& session, ServerTCP& tcpLayer);

private:
   

    DBManager& dbManager; // Referencia a la instancia de DBManager


};

