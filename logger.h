#ifndef __LOGGER_H
#define __LOGGER_H

#include <iostream>
#include <stdio.h>
#include <time.h>
#include <string>
#include <cstdarg>

#define LOG_CLR_RED		"\e[0;31m"
#define LOG_CLR_GREEN		"\e[0;32m"
#define LOG_CLR_YELLOW		"\e[0;33m"
#define LOG_CLR_BLUE		"\e[0;34m"
#define LOG_CLR_PURPLE		"\e[0;35m"
#define LOG_CLR_CYAN		"\e[0;36m"
#define LOG_CLR_WHITE		"\e[0;37m"

class Logger {
	public:
		Logger( std::string filename );
		~Logger();
		
		//void write(std::string line);
		void info(const char* fmt, ...);
		void debug(const char *fmt, ...);
		void error(const char *fmt, ...);
		void disableDebug();
		
		bool debugging;
		
	private:
		FILE *mFile;
		std::string now();
		void write(const char * line, std::string color);
};

#endif