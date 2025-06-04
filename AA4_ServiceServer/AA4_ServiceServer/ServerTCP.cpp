#include "ServerTCP.h"

ServerTCP::ServerTCP(unsigned short port) :listenPort(port), running(false)
{
}

ServerTCP::~ServerTCP()
{
	stopListener();
}

bool ServerTCP::startListener()
{
	if (listener.listen(listenPort) != sf::Socket::Status::Done) {
		std::cerr << "Error: No s'ha pogut escoltar al port " << listenPort << std::endl;
		return false;
	}

	selector.add(listener);
	running = true;
	std::cout << "Servidor escoltant al port " << listenPort << std::endl;
	return true;
}

void ServerTCP::stopListener()
{
	if (!running) return;
	running = false;
	listener.close();
	selector.clear();
	for (sf::TcpSocket* client : clients) {
		delete client; // Liberar memoria
	}
	clients.clear();
	std::cout << "Servidor TCP aturat del port " << listenPort << std::endl;
}

void ServerTCP::update()
{
    if (!running) return;

    if (selector.wait()) { 
        // 1. Comprobar el listener para nuevas conexiones
        if (selector.isReady(listener)) {
             std::cout << "Listener is ready, accepting new client." << std::endl; 
            acceptNewClient(); // acceptNewClient llamará a onClientConnected
        }

        
        std::list<sf::TcpSocket*> clients_copy = clients; 

        for (sf::TcpSocket* client : clients_copy) {
            
            bool stillConnected = false;
            for (sf::TcpSocket* originalClient : clients) {
                if (originalClient == client) {
                    stillConnected = true;
                    break;
                }
            }
            if (!stillConnected) {
                continue; // El cliente fue removido, pasar al siguiente
            }

            if (selector.isReady(*client)) {
                sf::Packet packet;
                sf::Socket::Status status = client->receive(packet);

                if (status == sf::Socket::Status::Done) {
                   
                    if (onPacketReceived) { 
                        onPacketReceived(client, packet);
                    }
                }
                else if (status == sf::Socket::Status::Disconnected) {
                    std::cout << "Client disconnected: " << client->getRemoteAddress().value() << ":" << client->getRemotePort() << " from service port " << listenPort << std::endl;
                    removeClient(client); 
                }
               
            }
        }
    }
}

//
//bool ServerTCP::sendToClient(sf::TcpSocket* client, sf::Packet& packet)
//{
//	if (client->send(packet) == sf::Socket::Status::Done) {
//		return true;
//	}
//	return false;
//}

bool ServerTCP::sendToClient(sf::TcpSocket* client, sf::Packet& packet) {
    if (!client) return false; // Comprobación de seguridad
    std::cout << "[ServerTCP DEBUG] Intentando enviar paquete al puerto cliente: " << client->getRemotePort() << std::endl;
    sf::Socket::Status send_status = client->send(packet);
    if (send_status == sf::Socket::Status::Done) {
        std::cout << "[ServerTCP DEBUG] Envio al puerto " << client->getRemotePort() << " exitoso (Done)." << std::endl;
        return true;
    }
    else {
        std::cerr << "[ServerTCP DEBUG] Fallo al enviar al puerto " << client->getRemotePort() << ". Estado: " << static_cast<int>(send_status) << std::endl;
        return false;
    }
}

//std::vector<sf::TcpSocket*> ServerTCP::getConnectedClients()
//{
//	std::vector<sf::TcpSocket*> list;
//	std::unordered_map<sf::TcpSocket*, sf::TcpSocket*>::iterator it;
//
//	for (it = clients.begin(); it != clients.end(); it++) {
//		list.push_back(it->first);
//	}
//
//	return list;
//}



void ServerTCP::acceptNewClient()
{


	sf::TcpSocket* newClient = new sf::TcpSocket();


	if (listener.accept(*newClient) == sf::Socket::Status::Done) {

		clients.push_back(newClient);
		selector.add(*newClient);
		newClient->setBlocking(false);
		std::cout << "Nou client connectat desde: " << newClient->getRemoteAddress().value() << std::endl;

		if (onClientConnected) {
			onClientConnected(newClient);
		}
	}
	else {
		delete newClient;
		std::cerr << "Error acceptant nou client al servei del port " << listenPort << std::endl;
	}
}

void ServerTCP::removeClient(sf::TcpSocket* client, bool notify)
{

	if (notify && onClientDisconnected) {
		onClientDisconnected(client);
	}
	selector.remove(*client);
	clients.remove(client); 
	delete client; // Liberar memoria


}


