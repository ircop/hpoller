#include "config.h"

boost::property_tree::ptree readConf( std::string path )
{
	std::ifstream ifs(path.c_str());
	if( !ifs ) {
	    fprintf(stderr, "Can't open config file '%s'!\n", path.c_str() );
	    exit(-2);
	}
	boost::property_tree::ptree pt;
	boost::property_tree::read_json(ifs, pt);
	
	
	/**
	 * Some checks:
	 */
	if( !checkConfig( pt ) ) {
		fprintf(stderr, "Errors in config file, exiting.\n");
		exit(2);
	}
	
	return pt;
}

bool checkConfig( boost::property_tree::ptree config )
{
	if( config.count("models") == 0 ) {
		fprintf(stderr, "No models section in config\n");
		return false;
	}
	
	try {
		config.get<unsigned long>("switch_root_id");
		config.get<std::string>("logfile");
		config.get<bool>("debug");
	} catch (std::exception& error) {
		fprintf( stderr, "Config error: %s\n", error.what());
		exit(2);
	}
	
	//boost::property_tree::ptree models = config.get_child("models");
	BOOST_FOREACH( boost::property_tree::ptree::value_type &v, config.get_child("models") )
	{
		if( v.first.empty() ) {
			fprintf(stderr, "models child name is empty!");
			return false;
		}
		std::string name = v.first.data();
		
		// current model tree:
		ptree model = v.second;
		//ptree model = config.get_child("models").get_child( name );	// <- works
		if( model.count("minIdx") == 0 || model.count("maxIdx") == 0 ) {
			fprintf(stderr, "config: No minIdx or maxIdx for model '%s'\n", name.c_str() );
			return false;
		}
		
		try {
			model.get<int>("minIdx");
			model.get<int>("maxIdx");
			model.get<std::string>("community");
		} catch (std::exception& error) {
			fprintf(stderr, "Config error: %s (model %s)\n", error.what(), name.c_str() );
			return false;
		}
		
		// check oids_get & oids_walk section
		if( model.count("oids_get") == 0 ) {
			fprintf(stderr, "config: no oids_get section in config for %s\n", name.c_str());
			return false;
		}
		if( model.count("oids_walk") == 0 ) {
			fprintf(stderr, "config: no oids_walk section in config for %s\n", name.c_str());
			return false;
		}
	}
	
	
	return true;
}

/**
 *  Проверка числового значения
 */
inline bool isInteger(const std::string & s)
{
   if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;

   char * p ;
   strtol(s.c_str(), &p, 10) ;

   return (*p == 0) ;
}
