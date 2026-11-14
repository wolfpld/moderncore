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

static inline int FindExifOrientation( const uint8_t* data, size_t size )
{
    if( size < 8 ) return -1;
    const bool bigEndian = data[0] == 0x4D;
    if( !bigEndian && data[0] != 0x49 ) return -1;

    TiffReader rd{ .base = data, .size = size, .bigEndian = bigEndian };
    rd.pos = 2;
    uint16_t magic = 0, count = 0;
    uint32_t ifd0 = 0;
    if( !rd.ReadU16( magic ) || magic != 42 || !rd.ReadU32( ifd0 ) || !rd.Seek( ifd0 ) || !rd.ReadU16( count ) ) return -1;

    for( uint16_t i = 0; i < count; i++ )
    {
        uint16_t tag = 0, type = 0;
        uint32_t cnt = 0, value = 0;
        if( !rd.ReadU16( tag ) || !rd.ReadU16( type ) || !rd.ReadU32( cnt ) || !rd.ReadU32( value ) ) break;
        if( tag == 0x0112 && cnt == 1 && ( type == 3 || type == 4 ) )
        {
            int orientation;
            if( type == 4 ) orientation = (int)value;
            else orientation = bigEndian ? (int)( value >> 16 ) : (int)(uint16_t)value;
            return orientation;
        }
    }
    return 0;
}
