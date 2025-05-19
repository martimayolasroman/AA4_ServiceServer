#pragma once

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#define SERVER "127.0.0.1:3306"
#define USERNAME "root"
#define PASSWORD "1234"
#define DATABASE "DuckGameDB"


class DBManager
{
public:

	DBManager();

	bool connectDataBase(const std::string& server, const std::string& usuari,
		const std::string& contrasenya, const std::string& baseDeDades);

	void disconnectDB();

	bool userExist(const std::string& nickname);
	bool registerUser(const std::string& nickname, const std::string& password);
	bool validateUser(const std::string& nickname, const std::string& password);

private:

	sql::Driver* driver;
	sql::Connection* con;
};

