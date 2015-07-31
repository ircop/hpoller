#include "snmp.h"

Snmp::Snmp(Logger *log, int tout_ms)
{
	init_snmp("COLLECTOR");
	this->timeout = tout_ms * 1000;
	if( this->timeout <= 0 )
		this->timeout = 100000;
	this->Log = log;
}


void Snmp::walk( std::string _oid, std::string _ipaddr, std::string _community, int minIdx, int maxIdx, walkResult &res )
{
	void					*sesspointer;		// <-- an opaque pointer, not a struct pointer
	struct snmp_session		session, *sptr;
	struct snmp_pdu			*pdu;
	struct snmp_pdu			*response;
	netsnmp_variable_list	*vars;
	int						arg;
//	oid						*anOID;
//	size_t					*anOID_len;
	oid						name[MAX_OID_LEN];
	size_t					name_length;
	int						count;
	int						running;
	int						status = STAT_ERROR;
	int						check;
	int						exitval = 0;
	int						numprinted = 0;
	int						liberr, syserr;
	char					*errstr;
	
	// Ищем OID
	std::map<std::string, oStruct>::iterator it = this->oids.find(_oid);
	if( it == this->oids.end() ) {
		this->Log->error("Oid '%s' not found in preloaded cache!", _oid.c_str() );
		return;
	}
	oid *anOID = it->second._oid;
	size_t anOID_len = it->second._len;
	
	snmp_sess_init(&session);
	session.version = SNMP_VERSION_2c;
	session.timeout = this->timeout;
	session.peername = (char *) _ipaddr.c_str();
	session.community = (u_char *) _community.c_str();
	session.community_len = _community.length();
	
	SOCK_STARTUP;
	
	sesspointer = snmp_sess_open( &session );
	if( sesspointer == NULL ) {
		/* snmp session error */
		snmp_error(&session, &liberr, &syserr, &errstr);
		this->Log->error("SNMP session error: %s", errstr);
		free(errstr);
		return;
	}
	
	// зададим имя главного объекта
	memmove(name, anOID, anOID_len * sizeof(oid) );
	name_length = anOID_len;
	
	running = 1;
	while( running )
	{
		/*
		* create PDU for GETBULK request and add object name to request 
		*/
		pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
		pdu->non_repeaters = 0;
		pdu->max_repetitions = 10;		/* fill the packet */
		snmp_add_null_var(pdu, name, name_length);
		
		status = snmp_sess_synch_response(sesspointer, pdu, &response);
		if( status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR ) {
			for( vars = response->variables; vars; vars = vars->next_variable )
			{
				if( (vars->name_length < anOID_len) || 
						(memcmp(anOID, vars->name, anOID_len * sizeof(oid)) != 0) ) {
					// Not part of this subtree
					// this->Log->debug("Not part of this oid");
					running = 0;
					continue;
				}
				
				numprinted++;
				//print_variable(vars->name, vars->name_length, vars);
				char _buf[256];
				snprint_objid( _buf, sizeof(_buf), vars->name, vars->name_length);
				std::string buf = _buf;
				
				/**
				 * Parse buffer to find index:
				**/
				std::size_t found = buf.find_last_of(".");
				if( (int)found < 0 ) {
					//printf("Notfound!\n"); /TODO LOG/
					running = 0;
					continue;
				}
				std::string index = buf.substr(found+1);
				/*
				if( !isInteger(index) ) {
					// TODO LOG
					running = 0;
					continue;
				}
				*/
				
				char *ptr;
				long key = strtol( index.c_str(), &ptr, 10 );
				
				if( key < minIdx || key > maxIdx ) {
					running = 0;
					continue;
				}
				
				std::string ret = this->parseValue(vars->name, vars->name_length, vars);
				if( ret == "" )
					continue;
				
				res.insert(std::pair<long,std::string>(key,ret));
				if( (vars->type != SNMP_ENDOFMIBVIEW) &&
					(vars->type != SNMP_NOSUCHOBJECT) &&
					(vars->type != SNMP_NOSUCHINSTANCE)) {
					// not an exception value
					// if checl && snmp_iod_compare ................
						
						// Check if last var, and if so, save for next request
						if( vars->next_variable == NULL ) {
							memmove(name, vars->name, vars->name_length * sizeof(oid));
							name_length = vars->name_length;
						}
				} else {
					// An exception value, so stop
					running = 0;
				}
			}
		} else {
			running = 0;
			if( status == STAT_SUCCESS ) {
				this->Log->debug("Error [snmpget] from switch %s: %s", _ipaddr.c_str(), snmp_errstring(response->errstat) );
			} else {
				snmp_sess_error(sesspointer, &liberr, &syserr, &errstr);
				this->Log->debug("SNMP write error: %s. (%s)", errstr, _ipaddr.c_str() );
				free(errstr);
			}
		}
		if( response )
			snmp_free_pdu(response);
	}
	snmp_sess_close(sesspointer);
}

std::string Snmp::get( std::string _oid, std::string _ipaddr, std::string _community ) {
	
	std::string ret = "";
	
	void					*sesspointer;		// <-- an opaque pointer, not a struct pointer
	struct snmp_session 	session, *sptr;
	struct snmp_pdu 		*pdu;
	struct snmp_pdu 		*response;
	
	int liberr, syserr;
	char *errstr;
	
	// Ищем OID
	std::map<std::string, oStruct>::iterator it = this->oids.find(_oid);
	if( it == this->oids.end() ) {
		this->Log->error("Oid '%s' not found in preloaded cache!", _oid.c_str() );
		return "";
	}
	oid *anOID = it->second._oid;
	size_t anOID_len = it->second._len;
	
	struct variable_list *vars;
	int status;
	
	snmp_sess_init( &session );
	session.peername = (char *) _ipaddr.c_str();
	session.timeout = this->timeout;		// 1000 = 1s.
	session.version = SNMP_VERSION_2c;
	session.community = (u_char *) _community.c_str();
	session.community_len = _community.length();
	
	sesspointer = snmp_sess_open( &session );
	if( sesspointer == NULL ) {
		/* snmp session error */
		snmp_error(&session, &liberr, &syserr, &errstr);
		this->Log->error("SNMP session error: %s", errstr);
		free(errstr);
		return "";
	}
	
	sptr = snmp_sess_session(sesspointer);
	
	pdu = snmp_pdu_create(SNMP_MSG_GET);
	snmp_add_null_var( pdu, anOID, anOID_len);
	
	status = snmp_sess_synch_response(sesspointer, pdu, &response);
	if( status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR ) {
		
		// Обрабатываем результат
		for( vars = response->variables; vars; vars = vars->next_variable )
		{
			ret = this->parseValue( vars->name, vars->name_length, vars );
		}
	} else {
		if( status == STAT_SUCCESS ) {
			this->Log->debug("Error [snmpget] from switch %s: %s", _ipaddr.c_str(), snmp_errstring(response->errstat) );
		} else {
			snmp_sess_error(sesspointer, &liberr, &syserr, &errstr);
			this->Log->debug("SNMP write error: %s.\n", errstr);
			free(errstr);
		}
	}
	
	if( response )
		snmp_free_pdu(response);
	snmp_sess_close(sesspointer);
	return ret;
}

// Чтение и парсинг MIB-файлов - не thread-safe, поэтому это нужно сделать заранее,
// до распараллеливания программы
int Snmp::parseOids(boost::property_tree::ptree config)
{
	this->Log->info("Parsing snmp oids...");
	BOOST_FOREACH( boost::property_tree::ptree::value_type &v, config.get_child("models") )
	{
		// Модель:
		boost::property_tree::ptree model = v.second;
		// одиды для snmpget
		BOOST_FOREACH( boost::property_tree::ptree::value_type &vx, model.get_child("oids_get") )
		{
			std::string oid_string = vx.second.get<std::string>("oid");
			if( this->oids.find(oid_string) == this->oids.end() ) {
				// Пытаемся распарсить этот oid и, если всё ок - добавляем его в массив
				
				oStruct oidStruct;
				oidStruct._len = MAX_OID_LEN;
				
				if( read_objid(oid_string.c_str(), oidStruct._oid, &oidStruct._len) == 0 ) {
					this->Log->error("Can't parse oid: %s", oid_string.c_str() );
				} else {
					this->oids.insert( std::pair<std::string,oStruct>(oid_string, oidStruct) );
				}
			}
		}
		
		// и для snmpwalk
		BOOST_FOREACH( boost::property_tree::ptree::value_type &vx, model.get_child("oids_walk") )
		{
			std::string oid_string = vx.second.get<std::string>("oid");
			if( this->oids.find(oid_string) == this->oids.end() ) {
				// Пытаемся распарсить этот oid и, если всё ок - добавляем его в массив
				oStruct oidStruct;
				oidStruct._len = MAX_OID_LEN;
				if( read_objid(oid_string.c_str(), oidStruct._oid, &oidStruct._len) == 0 ) {
					this->Log->error("Can't parse oid: %s", oid_string.c_str() );
				} else {
					this->oids.insert( std::pair<std::string, oStruct>(oid_string, oidStruct) );
				}
			}
		}
	}
	this->Log->info("Done.");
	return this->oids.size();
}

std::string Snmp::parseValue( const oid *objid, size_t objidlen, const netsnmp_variable_list * variable )
{
	std::string ret = "";
	std::stringstream stream;
	switch( variable->type )
	{
		case ASN_OCTET_STR:
			ret = std::string( reinterpret_cast<const char *> (variable->val.string), variable->val_len );
			break;
		case ASN_UINTEGER:
		case ASN_COUNTER:
		case ASN_UNSIGNED64:
		case ASN_TIMETICKS:
		case ASN_GAUGE:
			ret = std::to_string( (long long unsigned int) ( * variable->val.integer &  0xffffffff )  );
			break;
		case ASN_COUNTER64:
			ret = std::to_string( (long long unsigned int) (((uint64_t)variable->val.counter64[0].high) << 32) +  (uint64_t)variable->val.counter64[0].low );
			break;
		case ASN_INTEGER:
		case ASN_INTEGER64:
			ret = std::to_string( (long long unsigned int) variable->val.integer );
			break;
		case ASN_DOUBLE:
			stream << * variable->val.doubleVal;
			ret = stream.str();
			break;
		case ASN_FLOAT:
			stream << * variable->val.floatVal;
			ret = stream.str();
			break;
		case ASN_IPADDRESS:
			ret = std::to_string((long long unsigned int) variable->val.string[0]) + "."
					+ std::to_string((long long unsigned int)variable->val.string[1]) + "."
					+ std::to_string((long long unsigned int)variable->val.string[2]) + "."
					+ std::to_string((long long unsigned int)variable->val.string[3]);
			break;
		default:
			this->Log->error("UNSUPPORTED VALUE TYPE!");
			return "";
	}
	return ret;
}

void Snmp::setTimeout(int sec)
{
	this->timeout = sec * 1000;
}

Snmp::~Snmp()
{
	
}
