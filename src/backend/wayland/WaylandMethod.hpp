#ifndef __WAYLANDMETHOD_HPP__
#define __WAYLANDMETHOD_HPP__

#include <assert.h>

#define Method( cls, func ) [](void* ptr, auto... args) { assert( ptr ); ((cls*)ptr)->func( args... ); }

#endif
