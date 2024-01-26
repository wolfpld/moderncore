#pragma once

#include <assert.h>

#define Method( func ) [](void* ptr, auto... args) { assert( ptr ); ((decltype(this))ptr)->func( args... ); }
