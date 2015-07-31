#ifndef __SNMP_H
#define __SNMP_H

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <string>
#include <sstream>
#include <map>
#include "logger.h"
#include "config.h"


typedef struct {
	oid		_oid[MAX_OID_LEN];
	size_t	_len;
} oStruct;
typedef std::map<long int, std::string> walkResult;

class Snmp {
	public:
		Snmp(Logger *log, int tout_ms);
		~Snmp();
		
		std::string get( std::string _oid, std::string _ipaddr, std::string _community );
		void walk( std::string _oid, std::string _ipaddr, std::string _community, int minIdx, int maxIdx, walkResult &res );
		void setTimeout(int sec);
		int parseOids(boost::property_tree::ptree config);
		
	private:
		std::string parseValue( const oid *objid, size_t objidlen, const netsnmp_variable_list * variable );
		int timeout;
		Logger *Log;
		std::map<std::string, oStruct> oids;
};


#endif