#pragma once


#include <SFML/Network.hpp>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <list>


class ServerTCP
{
public:

    // Callback types
    using ClientCallback = std::function<void(sf::TcpSocket*)>;
    using PacketCallback = std::function<void(sf::TcpSocket*, sf::Packet&)>;


    ServerTCP(unsigned short port);
    ~ServerTCP();

    bool startListener();
    void stopListener();

    void update();

    bool sendToClient(sf::TcpSocket* client, sf::Packet& packet);

    // Setters for callbacks
    void setOnClientConnected(const ClientCallback& callback) { onClientConnected = callback; }
    void setOnClientDisconnected(const ClientCallback& callback) { onClientDisconnected = callback; }
    void setOnPacketReceived(const PacketCallback& callback) { onPacketReceived = callback; }

    unsigned short getPort() const { return listenPort; }
   // std::vector<sf::TcpSocket*> getConnectedClients();


private:

    void acceptNewClient();
    void removeClient(sf::TcpSocket* client, bool notify = true);


    sf::TcpListener listener;
    sf::SocketSelector selector;
    std::list<sf::TcpSocket*> clients; // Lista de sockets de clientes conectados

    unsigned short listenPort;
    bool running;

    // Callbacks
    ClientCallback onClientConnected;
    ClientCallback onClientDisconnected;
    PacketCallback onPacketReceived;

};

