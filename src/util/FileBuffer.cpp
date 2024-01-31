#include <format>

#include "FileBuffer.hpp"
#include "FileWrapper.hpp"
#include "util/Logs.hpp"

FileBuffer::FileBuffer( const char* fn )
    : m_buffer( nullptr )
    , m_size( 0 )
{
    FileWrapper file( fn, "rb" );
    if( !file )
    {
        mclog( LogLevel::Error, "Failed to open file: %s", fn );
        throw FileException( std::format( "Failed to open file: {}", fn ) );
    }

    mclog( LogLevel::Debug, "Loading file: %s", fn );

    fseek( file, 0, SEEK_END );
    m_size = ftell( file );
    fseek( file, 0, SEEK_SET );

    m_buffer = new char[m_size];
    file.Read( m_buffer, m_size );
}

FileBuffer::~FileBuffer()
{
    delete[] m_buffer;
}
