#include "logger.h"

Logger::Logger( std::string filename )
{
	this->debugging = true;
	this->mFile = fopen(filename.c_str(), "a");
	if( this->mFile == NULL ) {
		fprintf(stderr, "Can't open log file! (%s)\nLogging disabled.\n", filename.c_str() );
	}
}

void Logger::write(const char *line, std::string color)
{
	if( this->mFile == NULL )
		return;
	
	std::string str = color + std::string("[") + this->now() + std::string("] ") + std::string(line) + std::string("\n");
	fwrite( str.c_str(), sizeof(char), str.size(), this->mFile );
	fflush(this->mFile);
	
	if( this->debugging ) {
		printf("%s", str.c_str() );
	}
}

/* for default color */
void Logger::info(const char *fmt, ...)
{
	char dest[1024*16];
	va_list argpt;
	va_start(argpt, fmt);
	vsprintf(dest, fmt, argpt);
	va_end(argpt);
	
	this->write( dest, LOG_CLR_CYAN );
}

void Logger::debug(const char *fmt, ...)
{
	if( !this->debugging )
		return;
	
	char dest[1024*16];
	va_list argpt;
	va_start(argpt, fmt);
	vsprintf(dest, fmt, argpt);
	va_end(argpt);
	
	std::string line = std::string("[DEBUG] ") + std::string(dest);
	this->write( line.c_str(), LOG_CLR_YELLOW );
}

void Logger::error(const char *fmt, ...)
{
	if( !this->debugging )
		return;
	
	char dest[1024*16];
	va_list argpt;
	va_start(argpt, fmt);
	vsprintf(dest, fmt, argpt);
	va_end(argpt);
	
	std::string line = std::string("[ERROR] ") + std::string(dest);
	this->write( line.c_str(), LOG_CLR_RED );
}

void Logger::disableDebug()
{
	this->debugging = false;
}

std::string Logger::now()
{
	time_t		now = time(0);
	struct tm	tstruct;
	char		buf[80];
	tstruct		= *localtime(&now);
	strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &tstruct);
	
	return buf;
}

Logger::~Logger()
{
	fclose(this->mFile);
}
