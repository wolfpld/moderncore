#pragma once

#include <concepts>
#include <stddef.h>

template<typename T>
concept DataContainer = requires( T t )
{
    { t.size() } -> std::convertible_to<size_t>;
    { t.data() } -> std::convertible_to<const void*>;
};
