#include "ServerTCP.h"

ServerTCP::ServerTCP(unsigned short port) :listenPort(port), running(false)
{
}

ServerTCP::~ServerTCP()
{
	stopListener();
}

// Inicia el listener TCP y configura el SocketSelector
bool ServerTCP::startListener()
{
	if (listener.listen(listenPort) != sf::Socket::Status::Done) {
		std::cerr << "Error: No s'ha pogut escoltar al port " << listenPort << std::endl;
		return false;
	}

	selector.add(listener); // Añade el listener al selector
	running = true;
	std::cout << "Servidor escoltant al port " << listenPort << std::endl;
	return true;
}


// Cierra el listener y borra todos los clientes
void ServerTCP::stopListener()
{
	if (!running) return;
	running = false;
	listener.close();	// Deja de escuchar
	selector.clear();	// Limpia el SocketSelector
	for (sf::TcpSocket* client : clients) {
		delete client; // Liberar memoria
	}
	clients.clear();
	std::cout << "Servidor TCP aturat del port " << listenPort << std::endl;
}


// Este método se llama en bucle desde Server::run()
// Atiende nuevas conexiones y paquetes entrantes
void ServerTCP::update()
{
    if (!running) return;


	// Espera a que haya actividad en cualquiera de los sockets
    if (selector.wait()) { 
		// 1.	Si listener está listo, hay una nueva conexión pendiente
        if (selector.isReady(listener)) {
             std::cout << "Listener is ready, accepting new client." << std::endl; 
            acceptNewClient(); // // Acepta y notifica vía callback
        }

		// 2.	Copiamos la lista de clientes para iterar con seguridad
        std::list<sf::TcpSocket*> clients_copy = clients; 

		// Recorremos cada cliente: si está listo (disponible en selector), recibimos paquete
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

				// Llama al callback asignado en Server::run()
                if (status == sf::Socket::Status::Done) {
                   
                    if (onPacketReceived) { 
                        onPacketReceived(client, packet);
                    }
                }
                else if (status == sf::Socket::Status::Disconnected) {
                    std::cout << "Client disconnected: " << client->getRemoteAddress().value() << ":" << client->getRemotePort() << " from service port " << listenPort << std::endl;
                    removeClient(client); // Remueve al cliente y llama a onClientDisconnected
                }
               
            }
        }
    }
}


//Envía un sf::Packet a un cliente específico.
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




// Acepta una nueva conexión TCP entrante
void ServerTCP::acceptNewClient()
{


	sf::TcpSocket* newClient = new sf::TcpSocket();


	if (listener.accept(*newClient) == sf::Socket::Status::Done) {

		clients.push_back(newClient);
		selector.add(*newClient);
		newClient->setBlocking(false);
		std::cout << "Nou client connectat desde: " << newClient->getRemoteAddress().value() << std::endl;
		// Notifica a quien haya registrado onClientConnected
		if (onClientConnected) {
			onClientConnected(newClient);
		}
	}
	else {
		delete newClient;
		std::cerr << "Error acceptant nou client al servei del port " << listenPort << std::endl;
	}
}

//Elimina un cliente de la gestión de ServerTCP.
void ServerTCP::removeClient(sf::TcpSocket* client, bool notify)
{

	if (notify && onClientDisconnected) {
		onClientDisconnected(client);
	}
	selector.remove(*client);
	clients.remove(client); 
	delete client; // Liberar memoria


}


