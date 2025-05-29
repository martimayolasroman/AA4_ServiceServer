#pragma once

#include <SFML/Network.hpp>
#include <string>
#include "ClientSession.h"
#include "Utils.h"   
#include "ServerTCP.h"     
#include <fstream>
#include <iostream>


class LauncherService
{

public:

    LauncherService(const std::string& mapFilePath);
    void sendMapToClient(ClientSession& session, ServerTCP& tcpLayer);

private:

    std::string mapFilePath;



};

