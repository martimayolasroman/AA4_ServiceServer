#include "ServerTCP.h"

ServerTCP::ServerTCP(unsigned short port) :listenPort(port), running(false)
{
}

bool ServerTCP::start()
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

void ServerTCP::stop()
{
	running = false;
	listener.close();
	clients.clear();
}

void ServerTCP::update()
{
	if (selector.wait()) {
		if (selector.isReady(listener)) {
			acceptNewClient();
		}


	}
}

bool ServerTCP::sendToClient(sf::TcpSocket* client, sf::Packet& packet)
{
	if (client->send(packet) == sf::Socket::Status::Done) {
		return true;
	}
	return false;
}



std::vector<sf::TcpSocket*> ServerTCP::getConnectedClients()
{
	std::vector<sf::TcpSocket*> list;
	std::unordered_map<sf::TcpSocket*, sf::TcpSocket*>::iterator it;

	for (it = clients.begin(); it != clients.end(); it++) {
		list.push_back(it->first);
	}

	return list;
}



void ServerTCP::acceptNewClient()
{


	sf::TcpSocket* newClient = new sf::TcpSocket();


	if (listener.accept(*newClient) == sf::Socket::Status::Done) {
		newClient->setBlocking(false);
		selector.add(*newClient);

		clients[newClient] = newClient;
		std::cout << "Nou client connectat desde: " << newClient->getRemoteAddress().value() << std::endl;
	}
	else {
		delete newClient;
	}
}


