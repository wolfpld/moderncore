#pragma once

#include "util/Panic.hpp"

#define Method( func ) [](void* ptr, auto... args) { CheckPanic( ptr, "Class pointer is nullptr" ); ((decltype(this))ptr)->func( args... ); }

#define Invoke( func, ... ) if( m_listener->func ) { m_listener->func( m_listenerPtr, ##__VA_ARGS__ ); }