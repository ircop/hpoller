#ifndef __CONFIG_H
#define __CONFIG_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
 
#include <fstream>
#include <iostream>
#include <string>
#include "logger.h"

using namespace boost::property_tree;

boost::property_tree::ptree readConf( std::string path );
bool checkConfig( boost::property_tree::ptree config );

#endif