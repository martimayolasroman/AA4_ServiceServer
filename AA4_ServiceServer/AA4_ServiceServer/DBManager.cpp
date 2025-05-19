#include "DBManager.h"

DBManager::DBManager()
{
	driver = nullptr;
	con = nullptr;

}

bool DBManager::connectDataBase(const std::string& server, const std::string& usuari, const std::string& contrasenya, const std::string& baseDeDades)
{
	try {

		driver = get_driver_instance();
		con = driver->connect(server, usuari, contrasenya);
		con->setSchema(baseDeDades);
		std::cout << "DB -- Connection done. " << std::endl;


	}
	catch (sql::SQLException e) {

		std::cout << "DB -- Could not connect to server. Error message: " << e.what() << std::endl;



	}
}

void DBManager::disconnectDB()
{

	con->close();

	if (con->isClosed()) {


		delete con;
		con = nullptr;
		std::cout << "DB -- Connexió tancada." << std::endl;
	}


}

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


		/*sql::Statement* stmt = con->createStatement();
		std::string query = "SELECT id FROM usuaris WHERE nickname = '"+ nickname + "'";
		sql::ResultSet* res = stmt->executeQuery(query);
		bool existeix = res->next();
		delete res;
		delete stmt;
		return existeix;*/
	}
	catch (sql::SQLException& e) {
		std::cout << "DB -- Error comprovant usuari: " << e.what() << std::endl;
		return false;
	}
}

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



		/*sql::Statement* stmt = con->createStatement();
		std::string query = "INSERT INTO usuaris (nickname, password) VALUES ('" + nickname + "', '" + password + "')";
		int affected_rows = stmt->executeUpdate(query);
		delete stmt;
		if (affected_rows > 0)
		{
			std::cout << "DB -- User created successfully. " << std::endl;
			return true;
		}*/

	}
	catch (sql::SQLException& e) {
		std::cout << "DB -- Error registrant usuari: " << e.what() << std::endl;
		return false;
	}



	return false;
}

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



		/*sql::Statement* stmt = con->createStatement();
		std::string query = "SELECT id FROM usuaris WHERE nickname = '" + nickname + "' AND password = '" + password + "'";
		sql::ResultSet* res = stmt->executeQuery(query);
		bool valid = res->next();
		if (valid) {
			std::cout << "DB -- Usuari validat correctament " << std::endl;
		}
		else {
			std::cout << "DB -- Usuari o contrasenya incorrectes " << std::endl;
		}
		delete res;
		delete stmt;

		return valid;*/
	}
	catch (sql::SQLException& e) {
		std::cout << "DB -- Error validant usuari: " << e.what() << std::endl;
		return false;
	}
}


