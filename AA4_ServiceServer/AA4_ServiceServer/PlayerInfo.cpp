#include "PlayerInfo.h"



PlayerInfo::PlayerInfo(const std::string& nickname, const sf::IpAddress& ip, unsigned short port)
	:nickname(nickname), ip(ip), port(port), connected(true), roomId("")
{
}

PlayerInfo::PlayerInfo() :nickname(""), ip(sf::IpAddress(0, 0, 0, 0)), port(0), connected(false), roomId("")
{

}

const std::string PlayerInfo::getNickName() const
{
	return nickname;
}

const sf::IpAddress PlayerInfo::getIp() const
{
	return ip;
}

const unsigned short PlayerInfo::getPort() const
{
	return port;
}

void PlayerInfo::setConnected(bool status)
{
	connected = status;
}

bool PlayerInfo::isConnected() const
{
	return connected;
}

void PlayerInfo::setRoomId(const std::string& _roomId)
{
	roomId = _roomId;
}

const std::string PlayerInfo::getRoomId()
{
	return roomId;
}


