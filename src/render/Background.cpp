#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "Background.hpp"
#include "../util/Config.hpp"

Background::Background()
    : m_color {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}
{
    Config cfg( "background.ini" );
    const auto cstr = cfg.Get( "Background", "Color", "404040" );
    if( strlen( cstr ) == 6 )
    {
        const auto color = strtol( cstr, nullptr, 16 );
        if( color != LONG_MAX && color != LONG_MIN )
        {
            assert( ( color & ~0xFFFFFF ) == 0 );

            m_color.color.float32[0] = ( color >> 16 ) / 255.0f;
            m_color.color.float32[1] = ( ( color >> 8 ) & 0xFF ) / 255.0f;
            m_color.color.float32[2] = ( color & 0xFF ) / 255.0f;
        }
    }
}

Background::~Background()
{
}
