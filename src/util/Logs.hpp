#ifndef __LOGS_HPP__
#define __LOGS_HPP__

#include <stddef.h>
#include <string>

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const char* fmt, ... );
void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const std::string& str );

#define mclog(level, fmt, ...) MCoreLogMessage( level, __FILE__, __LINE__, fmt, ##__VA_ARGS__ )

#endif
