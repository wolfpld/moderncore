#pragma once

#include <stddef.h>

static inline size_t PixelCount( size_t width, size_t height )
{
    return width * height;
}

static inline size_t PixelChannelCount( size_t width, size_t height, size_t channels = 4 )
{
    return PixelCount( width, height ) * channels;
}

template<typename T>
static inline T* PixelAlloc( size_t width, size_t height, size_t channels = 4 )
{
    const auto size = PixelChannelCount( width, height, channels );
    return new T[size];
}
