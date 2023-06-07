#ifndef __PANIC_HPP__
#define __PANIC_HPP__

#include <stdlib.h>

#include "Logs.hpp"

#define CheckPanic( condition, msg, ... ) \
    { \
        if( !(condition) ) \
        { \
            mclog( LogLevel::Fatal, msg, ##__VA_ARGS__ ); \
            abort(); \
        } \
    }

#endif
