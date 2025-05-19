#pragma once


#include <SFML/Network.hpp>
#include <unordered_map>
#include <iostream>


class ServerTCP
{
public:
    ServerTCP(unsigned short port);
    bool start();
    void stop();

    void update();

    bool sendToClient(sf::TcpSocket* client, sf::Packet& packet);


    std::vector<sf::TcpSocket*> getConnectedClients();


private:

    void acceptNewClient();


    sf::TcpListener listener;
    sf::SocketSelector selector;
    std::unordered_map<sf::TcpSocket*, sf::TcpSocket*> clients;

    unsigned short listenPort;
    bool running;

};

