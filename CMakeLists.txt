cmake_minimum_required(VERSION 2.8)
project(hpoller)

ADD_DEFINITIONS("-std=c++0x") 

set(hpoller_VERSION_MAJOR 0)
set(hpoller_VERSION_MINOR 1)

set(BOOST_COMPONENTS
	thread
	program_options)

include(FindOracle.cmake)
include(FindNetSnmp.cmake)

find_library(Oracle REQUIRED)
find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_library(NetSnmp REQUIRED)


include_directories(${ORACLE_INCLUDE_DIR})

set(SOURCES "main.cpp" "config.cpp" "db.cpp" "logger.cpp" "snmp.cpp")
add_executable(hpoller ${SOURCES})
target_link_libraries(hpoller -locci -lclntsh -lnetsnmp -lboost_thread ${ORACLE_LIBRARY})

#include_directories(${ORACLE_INCLUDE_DIR} ${Boost_INCLUDE_DIRS} ${LIBCONFIG_INCLUDE_DIR} ${LIBOPING_INCLUDE_DIR})
#
#add_executable(hpinger main.cpp)
#
#target_link_libraries(hpinger ${ORACLE_LIBRARY} ${Boost_LIBRARIES} ${LIBCONFIG_LIBRARIES} ${LIBOPING_LIBRARY})
