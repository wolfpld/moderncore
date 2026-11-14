#pragma once

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>

// Bounds-checked reader for a TIFF block. All TIFF/IFD offsets are block-relative.
struct TiffReader
{
    const uint8_t* base;
    size_t size;
    size_t pos = 0;
    bool bigEndian;

    bool Seek( size_t to )
    {
        if( to > size ) return false;
        pos = to;
        return true;
    }
    bool ReadU16( uint16_t& v )
    {
        if( size - pos < 2 ) return false;
        memcpy( &v, base + pos, 2 );
        if( bigEndian ) v = ntohs( v );
        pos += 2;
        return true;
    }
    bool ReadU32( uint32_t& v )
    {
        if( size - pos < 4 ) return false;
        memcpy( &v, base + pos, 4 );
        if( bigEndian ) v = ntohl( v );
        pos += 4;
        return true;
    }
    void Skip( size_t n ) { pos = size - pos < n ? size : pos + n; }
};
