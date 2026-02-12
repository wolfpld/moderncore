#pragma once

#include <stdexcept>
#include <string>

struct ArgParseException : public std::runtime_error { explicit ArgParseException( const std::string& msg ) : std::runtime_error( msg ) {} };

bool ParseBoolean( const char* str );
