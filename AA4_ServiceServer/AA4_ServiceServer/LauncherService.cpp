#include "LauncherService.h"

LauncherService::LauncherService(const std::string& path) : mapFilePath(path)
{


}

void LauncherService::sendMapToClient(ClientSession& session, ServerTCP& tcpLayer)
{
    std::ifstream mapFile(mapFilePath, std::ios::binary);
    if (!mapFile.is_open()) {
        std::cerr << "[LauncherLogic] Error: No se pudo abrir el archivo del mapa: " << mapFilePath << std::endl;
        sf::Packet errorPacket;
       
        errorPacket << PacketType::S_ERROR_GENERAL << "Map file not found on server.";
        tcpLayer.sendToClient(session.socket, errorPacket);
       
        return;
    }

    std::string mapContent((std::istreambuf_iterator<char>(mapFile)), std::istreambuf_iterator<char>());
    mapFile.close();

    sf::Packet mapPacket;
    mapPacket << PacketType::S_MAP_DATA;
    mapPacket << mapContent;

    if (tcpLayer.sendToClient(session.socket, mapPacket)) {
        std::cout << "[LauncherLogic] Mapa enviado a " << session.ipAddress.toString() << ":" << session.port << std::endl;
        // QUITAR: session.state = ClientState::AWAITING_CREDENTIALS;
        // El estado de la sesión se maneja fuera ahora, dependiendo de cuándo se llame a sendMapToClient.
        // En el nuevo flujo, el estado ya es LOGGED_IN cuando se llama a esto.

        //session.state = ClientState::AWAITING_CREDENTIALS; // Cliente listo para login/register
    }
    else {
        std::cerr << "[LauncherLogic] Error enviando mapa a " << session.ipAddress.toString() << ":" << session.port << std::endl;
       
    }

}
