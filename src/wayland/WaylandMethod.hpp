#pragma once

#include "util/Panic.hpp"

#define Method( func ) [](void* ptr, auto... args) { CheckPanic( ptr, "Class pointer is nullptr" ); ((decltype(this))ptr)->func( args... ); }
