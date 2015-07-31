#include "main.h"

#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <mutex>
#include <unistd.h>

#include <boost/thread.hpp>		// for threads
#include <boost/program_options.hpp>	// for cmdargs
#include <occi.h>			// for oracle

namespace po = boost::program_options;
boost::property_tree::ptree config;
void processSwitch( std::string modelName, std::string ipaddr, ptree models, Collector c );

void sendString( std::string str, Collector c );
bool checkOpenSocket( );
void connect();
void mainLoop( int workers, ptree models );
void workerThread( std::vector<Switch> switches, ptree models );

struct sockaddr_in host;
int sockId = -1;
int slen = sizeof(host);
std::string ip;
int port;
int workers;

std::vector <Collector> collectors;

std::mutex Mutex;		// for udp socket lock
Logger *Log;
DB *db;
Snmp *SNMP;
int loopInterval;

int main(int argc, char *argv[])
{
	po::options_description desc("Available options");
	desc.add_options()
		("help,h", "display this message")
		("daemon,d", "run as daemon")
		("config,c", po::value<std::string>()->default_value("/etc/hpoller.json"), "config file location")
		("pid,p", po::value<std::string>()->default_value("/var/run/hpoller.pid"), "pid file location")
	;
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);
	
	if( vm.count("help") ) {
		std::cout << desc << std::endl;
		exit(0);
	}
	
	// Читаем конфиг
	config = readConf( vm["config"].as<std::string>().c_str() );
	
	// Daemonizing
	bool dmn = false;
	if( vm.count("daemon") ) {
		dmn = true;
		int mpid = fork();
		if( mpid < 0 ) {
		    fprintf(stderr, "Fork failed!\n");
		    exit(-1);
		}
		
		if( mpid > 0 ) {
		    exit(0);	// parent exit
		}
		
		int pid1 = getpid();
		umask(0);
		int sid = setsid();
		if( sid < 0 ) {
		    fprintf(stderr, "setsid() failed!\n");
		    exit(-1);
		}
		
		pid_t pid = getpid();
		FILE *fp = fopen( vm["pid"].as<std::string>().c_str(), "w");
		if( fp ) {
			fprintf(fp, "%d\n", pid);
			fclose(fp);
		}
	}
	
	// Инициализируем лог
	std::string logfile = config.get<std::string>("logfile");
	Log = new Logger( logfile, dmn );
	Log->info("%s", "Starting program");
	if( !config.get<bool>("debug") )
		Log->disableDebug();
	
	// Соединение с БД гидры (и другие параметры)
	int snmp_timeout_ms;
	std::string db_login, db_pw, db_name;
	unsigned long switchRoot = 0;
	try {
		db_login = config.get<std::string>("db.login");
		db_pw = config.get<std::string>("db.password");
		db_name = config.get<std::string>("db.db");
		snmp_timeout_ms = config.get<int>("snmp_timeout_ms");
		workers = config.get<int>("workers");
		loopInterval = config.get<int>("loop_interval");
		switchRoot = config.get<unsigned long>("switch_root_id");
	} catch (std::exception& error) {
		Log->error("Config error: %s", error.what());
		fprintf(stderr, "Config error: %s", error.what());
		exit(2);
	}
	
	if( workers < 1 ) {
		Log->error("Workers count must be 1+");
		exit(6);
	}
	
	db = new DB( db_login, db_pw, db_name, Log, switchRoot );
	SNMP = new Snmp(Log, snmp_timeout_ms);
	int oids = SNMP->parseOids( config );
	Log->info("Parsed %i unique oids from config.", oids);
	
	
	/*
	walkResult res;
	SNMP->walk( "IF-MIB::ifHCInOctets", "10.10.10.42", "public", 1, 28, res );
	for( std::map<long, std::string>::iterator it = res.begin(); it!=res.end(); ++it ) {
		std::string idx = std::to_string((long long unsigned int)it->first);
		std::string value = it->second;
		//std::string send_str = "switch." + std::string("10.10.10.42") + std::string(".inOctets.") + idx + " " + value + " " + std::to_string((long long unsigned int)t) + "\n";
		std::string send_str = idx + ":" + value;
		
			Log->debug("%s", send_str.c_str());
		
	}
	*/
	//std::string res = SNMP->get("IF-MIB::ifHCInOctets.25", "10.10.10.42", "public");
	//std::string res = SNMP->get("DISMAN-EVENT-MIB::sysUpTimeInstance", "10.10.10.42", "public");
	//std::cout << res << std::endl;
	//exit(0);
	
	
	connect();
	
	mainLoop(workers, config.get_child("models") );
}

void mainLoop( int workers, ptree models )
{
	std::vector <Switch> allSwitches;
	int lastLoopTime = 0;
	int lastSwitchUpdateTime = 0;

	while(true)
	{
		int now = time(0);
		// Цикл - каждые `loopInterval` сек.
		if( lastLoopTime != 0 && (now - lastLoopTime) < loopInterval ) {
			int toSleep = ( (lastLoopTime + loopInterval) - now );
			if( toSleep > 0 ) {
				Log->debug("Sleeping %i sec. before next loop...", ( (lastLoopTime + loopInterval) - now ) );
				sleep( (lastLoopTime + loopInterval) - now );
			}
		}
		Log->debug("loop...");
		lastLoopTime = now;
		
		// Если последний раз свитчи обновлялись более (некоего времени) назад, ИЛИ ещё обновлялись вообще (time=0), 
		// пробуем обновить список свитчей.
		now = time(0);
		std::vector <Switch> newSwitches;
		if( lastSwitchUpdateTime == 0 || (now - lastSwitchUpdateTime) > 600 /* 10 min. */ )
		{
			Log->info("Updating switches from DB...");
			db->getSwitches(newSwitches);
			Log->info("Total switches count got: %i", newSwitches.size());
			
			// Если свитчи обновить удалось, обновляем время обновления свитчей и 
			// заполняем "старый" массив свитчей новыми данными
			if( newSwitches.size() > 0 ) {
				lastSwitchUpdateTime = time(0);
				allSwitches.clear();
				allSwitches = newSwitches;
				newSwitches.clear();
			}
		}
		
		// Если массив свитчей пуст -- спим какое-то время
		if( allSwitches.size() < 1 ) {
			Log->error("Switches array is empty! Sleeping 10 sec.");
			sleep(10);
			continue;
		}
		
		/** Распределяем свитчи по воркерам. */
		std::vector<std::vector<Switch>> worker_switches;
		worker_switches.resize( workers );
		int wrkr = 0;
		for( int i=0; i<allSwitches.size(); i++ ) {
			Switch current = allSwitches[i];
			worker_switches[wrkr].push_back(current);
			
			if( wrkr != (workers - 1) ) {
				wrkr++;
			} else {
				wrkr = 0;
			}
		}
		
		Log->debug("Switch per worker (%i workers): %i", workers, worker_switches[0].size() );
		
		// Start threads
		boost::thread_group threads;
		for( int child=0; child < workers; child++ ) {
			try {
				threads.create_thread( boost::bind(workerThread, worker_switches[child], models));
			} catch (std::exception& error) {
				Log->error("Error creating thread! %s", error.what());
			}
		}
		
		try {
			threads.join_all();
			threads.interrupt_all();
		} catch (std::exception& error) {
			Log->error("Error join/interrupt threads! %s", error.what());
		}
		
		//cleanup
		worker_switches.clear();
	}
}

void workerThread( std::vector<Switch> switches, ptree models )
{
	int collectorsNum = collectors.size();
	int collectorNow = 0;
	for( int i=0; i<switches.size(); i++ ) {
		Switch sw = switches[i];
		processSwitch(sw.model, sw.ip, models, collectors[collectorNow] );
		
		if( collectorNow == (collectors.size() - 1) ) {
			collectorNow = 0;
		} else {
			collectorNow++;
		}
	}
}

void processSwitch( std::string modelName, std::string ipaddr, ptree models, Collector c )
{
	//Log->debug("Processing %s (%s)...\n", ipaddr.c_str(), modelName.c_str() );
	
	std::string community		= "";
	int minIdx					= 0;
	int maxIdx					= 0;
	
	std::string Send = "";
	std::string ipcode = ipaddr;
	std::replace(ipcode.begin(), ipcode.end(), '.', '_');
	
	ptree model, oids_get;
//	Log->debug("before: %s", modelName.c_str() );
	std::replace(modelName.begin(), modelName.end(), '.', '_');
	std::replace(modelName.begin(), modelName.end(), ' ', '_');
//	Log->debug("after: %s", modelName.c_str() );

	if( models.count(modelName) == 0 ) {
		//Log->debug("No such model name in config (%s), skip.\n", modelName.c_str() );
		return;
	}
	
	try {
		model = models.get_child(modelName);
		minIdx = model.get<int>("minIdx");
		maxIdx = model.get<int>("maxIdx");
		community = model.get<std::string>("community");
		oids_get = model.get_child("oids_get");
	} catch(std::exception &e) {
		Log->error("processSwitch(%s, %s): %s", modelName.c_str(), ipaddr.c_str(), e.what() );
		exit(1);
		return;
	}
	
//	Log->debug("foreaching...");
	unsigned long t = std::time(0);
	BOOST_FOREACH( boost::property_tree::ptree::value_type &v, oids_get )
	{
		std::string code = v.second.get<std::string>("code");
		std::string oid = v.second.get<std::string>("oid");
		std::string value = SNMP->get(oid, ipaddr, community);
		std::string str = "switch." + ipcode + "." + code + " " + value + " " + std::to_string((long long unsigned int)t) + "\n";
		
		Send += str;
	}
//	Log->debug("done.");
	
	ptree oids_walk = model.get_child("oids_walk");
	BOOST_FOREACH( boost::property_tree::ptree::value_type &v, oids_walk )
	{
		std::string code = v.second.get<std::string>("code");
		std::string oid = v.second.get<std::string>("oid");
		
		walkResult res;
		SNMP->walk( oid, ipaddr, community, minIdx, maxIdx, res );
		t = std::time(0);
		for( std::map<long, std::string>::iterator it = res.begin(); it!=res.end(); ++it ) {
			std::string idx = std::to_string((long long unsigned int)it->first);
			std::string value = it->second;
			std::string send_str = "switch." + ipcode + "." + code + "." + idx + " " + value + " " + std::to_string((long long unsigned int)t) + "\n";
			Send += send_str;
		}
	}
	
	sendString( Send, c );
}

void sendString( std::string str, Collector c )
{
	Mutex.lock();
	//if( sendto(sockId, str.c_str(), str.size(), 0, (struct sockaddr *)&host, slen) == -1 )
	if( sendto(c.socketId, str.c_str(), str.size(), 0, (struct sockaddr *)&c.host, c.slen) == -1 )
	{
		fprintf(stderr, "cannot sendto() to udp socket!\n");
		exit(2);
	}
	Mutex.unlock();
}

void connect()
{
	// Read all collectors from config
	Log->debug("Reading collectors from config...");
	if( config.count("collectors") == 0 ) {
		Log->error("config.collectors section does not exists!");
		exit(7);
	}
	
	BOOST_FOREACH( boost::property_tree::ptree::value_type &v, config.get_child("collectors") )
	{
		std::string ip = v.second.get<std::string>("host");
		int port = v.second.get<int>("port");
		Log->info("Opening udp socket to %s:%i...", ip.c_str(), port );
		
		Collector c;
		c.socketId = -1;
		c.iPort = port;
		c.slen = sizeof(c.host);
		c.ip = ip;
		
		if( (c.socketId = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 ) {
			Log->error("Can't create udp socket!");
			exit(5);
		}
		memset( (char*) &c.host, 0, sizeof(c.host) );
		c.host.sin_family = AF_INET;
		c.host.sin_port = htons(c.iPort);
		if( inet_aton(c.ip.c_str(), &c.host.sin_addr) == 0 ) {
			Log->error("inet_aton() failed!");
			exit(2);
		}
		Log->info("Socket opened.");
		collectors.push_back(c);
	}
	if( collectors.size() < 1 ) {
		Log->error("Got %i collectors in config. Exiting.", collectors.size() );
		exit(-1);
	}
}
