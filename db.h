#ifndef __DB_H
#define __DB_H

#include <occi.h>							// for oracle
#include <unistd.h>							// for sleep()
#include <vector>
#include "logger.h"


using namespace oracle::occi;

typedef struct {
	std::string id;
	std::string ip;
	std::string model;
} Switch;

class DB {
	public:
		DB(std::string Login, std::string Pw, std::string Db, Logger *log, unsigned long root);
		~DB();
		
		bool connect();
		bool disconnect();
		
		int getSwitches(std::vector <Switch> &switches);
		
	private:
		Connection *conn;
		Environment *env;
		Logger *Log;
		std::string login, password, db;
		unsigned long switchRoot;
};

#endif
