#pragma once

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

void SetLogLevel( LogLevel level );

void MCoreLogMessage( LogLevel level, const char* fileName, size_t line, const char* fmt, ... );

#define mclog(level, fmt, ...) MCoreLogMessage( level, __FILE__, __LINE__, fmt, ##__VA_ARGS__ )
