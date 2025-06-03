#pragma once
#include <string>
#include <SFML/Network.hpp>


class PlayerInfo
{

public:
	PlayerInfo(); // constructor per defecte
	PlayerInfo(const std::string& nickname, const sf::IpAddress& ip, unsigned short port);


	// Getters / Setters
	const std::string getNickName() const;
	const sf::IpAddress getIp() const;
	const unsigned short getPort() const;


	void setConnected(bool status);
	bool isConnected() const;

	/*void setRoomId(const std::string& _roomId);
	const std::string getRoomId();*/


private:

	std::string nickname;		// Nickname del usuario
	sf::IpAddress ip;			 // IP del cliente
	unsigned short port;		// Puerto del cliente
	bool connected;				// Estado de conexión

	/*std::string roomId;*/


};

