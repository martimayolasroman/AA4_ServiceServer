
#include <SFML/Network.hpp>
#include <thread>
#include <atomic>
#include <iostream>
#include "ServerConfig.h"
#include "AuthService.h"
#include "LauncherService.h"
#include "MatchmakingService.h"
#include "Server.h"


// Flag global que controla cuándo detener todo el servidor
std::atomic<bool> global_server_running_flag(true);



int main() {

	// Cargar la configuración (puertos, rutas, IPs, etc.)
	ServerConfig config;
	Server server(config);
	std::cout << "Servidor  iniciado en el puerto: " << config.ServerPort << std::endl;

	// Arranca el servidor en un hilo separado para no bloquear la consola principal.
	std::thread serverThread(&Server::run, &server);
	std::cout << "[Main] Server::run() iniciado en un thread separado." << std::endl;

	std::cout << "\n[Main] Servidor servicios en ejecución." << std::endl;
	std::cout << "Escribe 'exit' y presiona ENTER para detener el servidor." << std::endl;

	// Bucle principal del proceso "main": solo lee comandos de consola.
	std::string command;
	while (global_server_running_flag){

		std::cout << "> ";

		if (std::getline(std::cin, command)) {
			

			if (command == "exit" ) {
				std::cout << "[Main] Comando de salida recibido." << std::endl;
				global_server_running_flag = false; // Señal para detener
			}
			else if (!command.empty()) {
				std::cout << "[Main] Comando desconocido: '" << command << "'. Escribe 'exit' o 'quit'." << std::endl;
			}
		}

		
		
	}
    
	std::cout << "\n[Main] Iniciando proceso de detención del servidor..." << std::endl;
	// Llama a Server::stop() para cerrar listener y notificar al hilo interno.
	server.stop();
	// Esperar a que el thread del servidor termine su ejecución.
	if (serverThread.joinable()) {
		std::cout << "[Main] Esperando que el thread del servidor finalice..." << std::endl;
		serverThread.join();
		std::cout << "[Main] Thread del servidor finalizado." << std::endl;
	}
	std::cout << "[Main] Servidor detenido correctamente." << std::endl;

	return 0;
}