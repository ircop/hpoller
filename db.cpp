#include "db.h"

DB::DB(std::string Login, std::string Pw, std::string Db, Logger *log, unsigned long root)
{
	this->conn = NULL;
	this->env;
	this->switchRoot = root;
	
	this->Log = log;
	this->login = Login;
	this->password = Pw;
	this->db = Db;
	
	try {
		this->env = Environment::createEnvironment(Environment::DEFAULT);
	} catch( SQLException &e ) {
		this->Log->error("Can't create environment: %s", e.getMessage().c_str() );
		exit(2);
	}
	
}

bool DB::connect()
{
	try {
		this->Log->debug("Connecting to db...");
		//this->Log->debug("params: %s %s %s", this->login.c_str(), this->password.c_str(), this->db.c_str());
		this->conn = env->createConnection(this->login.c_str(), this->password.c_str(), this->db.c_str());
		this->Log->debug("Connected to DB.");
	} catch( SQLException &e ) {
		this->Log->error("Oracle connection error!");
		this->Log->error("%s", e.getMessage().c_str() );
		return false;
	}
	
	return true;
}

bool DB::disconnect()
{
	try {
		this->env->terminateConnection(this->conn);
	} catch( SQLException &e ) {
		this->Log->error("Oracle disconnect error!");
		this->Log->error("%s", e.getMessage().c_str() );
		return false;
	}
	
	return true;
}

int DB::getSwitches(std::vector <Switch> &switches)
{
	if( !this->connect() )
		return -1;
	
	int count = 0;
	ResultSet* res;
	Statement* sth;

	try {
		sth = this->conn->createStatement("\
		SELECT COUNT(O.N_OBJECT_ID) \
			FROM SI_V_OBJECTS_SIMPLE O \
		INNER JOIN SR_V_GOODS_SIMPLE G \
		ON G.N_GOOD_TYPE_ID=1 AND G.N_GOOD_ID=O.N_GOOD_ID \
		INNER JOIN SR_V_GOODS_SIMPLE G2 \
		ON G2.N_GOOD_ID=G.N_PARENT_GOOD_ID \
			INNER JOIN SI_V_OBJECTS_SPEC_SIMPLE OSPEC ON OSPEC.N_MAIN_OBJECT_ID=O.N_OBJECT_ID  AND OSPEC.VC_NAME LIKE 'CPU %' \
			INNER JOIN SI_V_OBJ_ADDRESSES_SIMPLE_CUR IPADR ON IPADR.N_ADDR_TYPE_ID=SYS_CONTEXT('CONST', 'ADDR_TYPE_IP') AND IPADR.N_OBJECT_ID=OSPEC.N_OBJECT_ID \
				LEFT JOIN SI_V_OBJ_VALUES V1 ON V1.N_OBJECT_ID=O.N_OBJECT_ID AND V1.N_GOOD_VALUE_TYPE_ID=51820501 \
			WHERE G2.N_PARENT_GOOD_ID=:root \
			AND V1.VC_VISUAL_VALUE='Y' \
			/* AND IPADR.VC_VISUAL_CODE = '10.170.199.58' */ \
		");
		sth->setUInt(1, this->switchRoot);
		res = sth->executeQuery();
		if( res->next() )
			count = res->getInt(1);
		else
			throw "Can't do res->next()!";
		
		sth->closeResultSet(res);
		this->conn->terminateStatement(sth);
		
		sth = this->conn->createStatement("\
			SELECT TO_CHAR(O.N_OBJECT_ID), IPADR.VC_VISUAL_CODE AS IP, \
			G.VC_GOOD_NAME AS MODEL \
				FROM SI_V_OBJECTS_SIMPLE O \
			INNER JOIN SR_V_GOODS_SIMPLE G \
			ON G.N_GOOD_TYPE_ID=1 AND G.N_GOOD_ID=O.N_GOOD_ID \
			INNER JOIN SR_V_GOODS_SIMPLE G2 \
			ON G2.N_GOOD_ID=G.N_PARENT_GOOD_ID \
				INNER JOIN SI_V_OBJECTS_SPEC_SIMPLE OSPEC ON OSPEC.N_MAIN_OBJECT_ID=O.N_OBJECT_ID  AND OSPEC.VC_NAME LIKE 'CPU %' \
				INNER JOIN SI_V_OBJ_ADDRESSES_SIMPLE_CUR IPADR ON IPADR.N_ADDR_TYPE_ID=SYS_CONTEXT('CONST', 'ADDR_TYPE_IP') AND IPADR.N_OBJECT_ID=OSPEC.N_OBJECT_ID \
					LEFT JOIN SI_V_OBJ_VALUES V1 ON V1.N_OBJECT_ID=O.N_OBJECT_ID AND V1.N_GOOD_VALUE_TYPE_ID=51820501 \
				WHERE G2.N_PARENT_GOOD_ID=:root  \
				AND V1.VC_VISUAL_VALUE='Y' \
				/* AND IPADR.VC_VISUAL_CODE = '10.170.199.58' */ \
		");
		sth->setUInt(1, this->switchRoot);
		res = sth->executeQuery();
		
		while( res->next() ) {
			Switch sw;
			
			sw.id = res->getString(1);
			sw.ip = res->getString(2);
			sw.model = res->getString(3);
			
			switches.push_back(sw);
			//this->Log->debug("Switch: %s | %s | %s", id.c_str(), ip.c_str(), model.c_str());
		}
		
		//this->Log->debug("swithces size: %i", (int)switches.size() );
		
		sth->closeResultSet(res);
		this->conn->terminateStatement(sth);
		
		
	} catch( SQLException &e ) {
		this->Log->error("Oracle query error!");
		this->Log->error("%s", e.getMessage().c_str() );
		this->disconnect();
		return -1;
	}
	
	this->disconnect();
	return count;
}

DB::~DB()
{
	this->disconnect();
}
