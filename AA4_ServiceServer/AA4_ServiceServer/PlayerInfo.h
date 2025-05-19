#pragma once
#include <string>
#include <SFML/Network.hpp>


class PlayerInfo
{

public:
	PlayerInfo(); // constructor per defecte
	PlayerInfo(const std::string& nickname, const sf::IpAddress& ip, unsigned short port);

	const std::string getNickName() const;
	const sf::IpAddress getIp() const;
	const unsigned short getPort() const;


	void setConnected(bool status);
	bool isConnected() const;

	void setRoomId(const std::string& _roomId);
	const std::string getRoomId();


private:

	std::string nickname;
	sf::IpAddress ip;
	unsigned short port;
	bool connected;

	std::string roomId;


};

