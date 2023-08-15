#pragma once

#include <assert.h>

#define Method( cls, func ) [](void* ptr, auto... args) { assert( ptr ); ((cls*)ptr)->func( args... ); }
