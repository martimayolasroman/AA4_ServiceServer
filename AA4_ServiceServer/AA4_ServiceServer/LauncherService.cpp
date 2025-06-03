#include "LauncherService.h"

LauncherService::LauncherService(const std::string& path) : mapFilePath(path)
{


}

void LauncherService::sendMapToClient(ClientSession& session, ServerTCP& tcpLayer)
{

    // Lee el archivo de mapa completo en memoria
    std::ifstream mapFile(mapFilePath, std::ios::binary);
    if (!mapFile.is_open()) {
        std::cerr << "[LauncherLogic] Error: No se pudo abrir el archivo del mapa: " << mapFilePath << std::endl;
        sf::Packet errorPacket;
       
        errorPacket << PacketType::S_ERROR_GENERAL << "Map file not found on server.";
        tcpLayer.sendToClient(session.socket, errorPacket);
       
        return;
    }

    // Lee todo el contenido como una cadena
    std::string mapContent((std::istreambuf_iterator<char>(mapFile)), std::istreambuf_iterator<char>());
    mapFile.close();


    // Prepara paquete: primero tipo S_MAP_DATA, luego todo el contenido del mapa
    sf::Packet mapPacket;
    mapPacket << PacketType::S_MAP_DATA;
    mapPacket << mapContent;

    if (tcpLayer.sendToClient(session.socket, mapPacket)) {
        std::cout << "[LauncherLogic] Mapa enviado a " << session.ipAddress.toString() << ":" << session.port << std::endl;
        // Una vez enviado el mapa, el cliente pasará a enviar login/registro
        session.state = ClientState::AWAITING_CREDENTIALS; // Cliente listo para login/register
    }
    else {
        std::cerr << "[LauncherLogic] Error enviando mapa a " << session.ipAddress.toString() << ":" << session.port << std::endl;
       
    }

}
