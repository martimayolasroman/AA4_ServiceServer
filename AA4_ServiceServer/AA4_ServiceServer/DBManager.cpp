#include "DBManager.h"

DBManager::DBManager()
{
	driver = nullptr;
	con = nullptr;

}

//Establece una conexión con la base de datos MySQL.
bool DBManager::connectDataBase(const std::string& server, const std::string& usuari, const std::string& contrasenya, const std::string& baseDeDades)
{
	try {

		driver = get_driver_instance();
		con = driver->connect(server, usuari, contrasenya);
		con->setSchema(baseDeDades);
		isConnected = true;
		std::cout << "DB -- Connection done. " << std::endl;


	}
	catch (sql::SQLException e) {

		std::cout << "DB -- Could not connect to server. Error message: " << e.what() << std::endl;



	}
}
//Cierra la conexión activa con la base de datos.
void DBManager::disconnectDB()
{

	con->close();

	if (con->isClosed()) {


		delete con;
		con = nullptr;
		std::cout << "DB -- Connexió tancada." << std::endl;
	}


}
//Comprueba si un usuario con el nickname dado ya existe en la tabla usuaris.
bool DBManager::userExist(const std::string& nickname)
{
	try {


		sql::PreparedStatement* stmt = con->prepareStatement("SELECT id FROM usuaris WHERE nickname = ?");
		stmt->setString(1, nickname);

		sql::ResultSet* res = stmt->executeQuery();
		bool exist = res->next();
		delete res;
		delete stmt;
		return exist;


		
	}
	catch (sql::SQLException& e) {
		std::cout << "DB -- Error comprovant usuari: " << e.what() << std::endl;
		return false;
	}
}
//Inserta un nuevo usuario con nickname y password en la tabla usuaris.
bool DBManager::registerUser(const std::string& nickname, const std::string& password)
{

	try {

		sql::PreparedStatement* stmt = con->prepareStatement("INSERT INTO usuaris (nickname, password) VALUES (?, ?)");
		stmt->setString(1, nickname);
		stmt->setString(2, password);
		int affected_rows = stmt->executeUpdate();
		delete stmt;
		if (affected_rows > 0)
		{
			std::cout << "DB -- User created successfully. " << std::endl;
			return true;
		}




	}
	catch (sql::SQLException& e) {
		std::cout << "DB -- Error registrant usuari: " << e.what() << std::endl;
		return false;
	}



	return false;
}

//Verifica si el nickname y password proporcionados coinciden con un registro en la tabla usuaris.
bool DBManager::validateUser(const std::string& nickname, const std::string& password)
{
	try {

		sql::PreparedStatement* stmt = con->prepareStatement("SELECT id FROM usuaris WHERE nickname = ? AND password = ?");
		stmt->setString(1, nickname);
		stmt->setString(2, password);

		sql::ResultSet* res = stmt->executeQuery();
		bool valid = res->next();
		if (valid) {
			std::cout << "DB -- Usuari validat correctament " << std::endl;
		}
		else {
			std::cout << "DB -- Usuari o contrasenya incorrectes " << std::endl;
		}
		delete res;
		delete stmt;

		return valid;



		
	}
	catch (sql::SQLException& e) {
		std::cout << "DB -- Error validant usuari: " << e.what() << std::endl;
		return false;
	}
}


