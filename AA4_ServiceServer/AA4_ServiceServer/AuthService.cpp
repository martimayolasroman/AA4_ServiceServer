#include "AuthService.h"



AuthService::AuthService(DBManager& databaseManager) : dbManager(databaseManager)
{

}




// Procesa la petición de login o registro.
// 'packet_type' es el tipo ya extraído (C_REQUEST_LOGIN o C_REQUEST_REGISTER).
// 'packet_data' es el paquete SFML del cual extraer nickname y password.
// Actualiza session.state y session.playerInfo en caso de éxito.
// Envía la respuesta al cliente usando tcp.

void AuthService::processAuthenticationRequest(PacketType packet_type, sf::Packet& packet_data, 
    ClientSession& session, ServerTCP& tcpLayer)
{

	std::string nickname, password;
	packet_data >> nickname >> password;
	sf::Packet responsePacket;
	bool authSuccess = false;


    if (packet_type == C_REQUEST_LOGIN) {
        std::cout << "[AuthLogic] LOGIN: User=" << nickname << " from " << session.ipAddress.toString() << std::endl;
        if (dbManager.validateUser(nickname, password)) {
            session.playerInfo = PlayerInfo(nickname, session.ipAddress, session.port);
            session.playerInfo.setConnected(true); // Marcar como conectado en PlayerInfo
            session.state = ClientState::LOGGED_IN;
            responsePacket << S_LOGIN_OK;
           
            authSuccess = true;
        }
        else {
            responsePacket << S_LOGIN_FAIL;
        }
    }
    else if (packet_type == C_REQUEST_REGISTER) {
        std::cout << "[AuthLogic] REGISTER: User=" << nickname << " from " << session.ipAddress.toString() << std::endl;
        if (dbManager.userExist(nickname)) {
            responsePacket <<S_REGISTER_FAIL ; // Código de error: Usuario ya existe
        }
        else {
            if (dbManager.registerUser(nickname, password)) {
                session.playerInfo = PlayerInfo(nickname, session.ipAddress, session.port);
                session.playerInfo.setConnected(true);
                session.state = ClientState::LOGGED_IN; // Auto-login después del registro
                responsePacket << S_REGISTER_OK;
                authSuccess = true;
            }
            else {
                responsePacket << S_REGISTER_FAIL ; // Código de error: Fallo BBDD
            }
        }
    }
    else {
        std::cerr << "[AuthLogic] Tipo de paquete de autenticación inesperado: "  << std::endl;
        responsePacket << S_ERROR_GENERAL << "Invalid authentication request type.";
    }

    tcpLayer.sendToClient(session.socket, responsePacket);

    if (authSuccess) {
        std::cout << "[AuthLogic] Autenticación exitosa para: " << nickname << std::endl;
    }
    else {
        std::cout << "[AuthLogic] Fallo de autenticación para: " << nickname << std::endl;
    }
}








