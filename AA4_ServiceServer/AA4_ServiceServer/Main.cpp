
#include <SFML/Network.hpp>
#include <iostream>
#include "Server.h"

#define LINTENER_PORT 55000



int main() {

	std::cout << "Inicialitzant servidor de Parchis al port " << LINTENER_PORT << "..." << std::endl;
	Server servidor(LINTENER_PORT);
	servidor.run();
	std::cout << "Servidor aturat correctament." << std::endl;
	return 0;
}