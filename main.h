#ifndef __MAIN_H
#define __MAIN_H

#include <vector>
#include "config.h"
#include "logger.h"
#include "db.h"
#include "snmp.h"

typedef struct {
	int socketId;
	struct sockaddr_in host;
	//int slen = sizeof(host);
	int slen;
	int iPort;
	std::string ip;
} Collector;

#endif
