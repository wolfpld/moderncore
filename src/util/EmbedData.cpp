#include <lz4.h>

#include "util/EmbedData.hpp"
#include "util/Panic.hpp"

EmbedData::EmbedData( size_t size, size_t lz4Size, const uint8_t* data )
    : DataBuffer( new char[size], size )
{
    const auto dec = LZ4_decompress_safe( (const char*)data, (char*)m_data, lz4Size, size );
    CheckPanic( dec == size, "Failed to decompress embedded data" );
}

EmbedData::~EmbedData()
{
    delete[] m_data;
}
