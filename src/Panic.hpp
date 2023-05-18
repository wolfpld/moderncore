#ifndef __PANIC_HPP__
#define __PANIC_HPP__

#include <stdlib.h>

extern "C" {
#include <wlr/util/log.h>
}

#define CheckPanic( condition, msg ) \
    { \
        if( !(condition) ) \
        { \
            wlr_log( WLR_ERROR, "%s", msg ); \
            abort(); \
        } \
    }

#endif
