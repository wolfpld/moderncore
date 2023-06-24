#ifndef __FILEBUFFER_HPP__
#define __FILEBUFFER_HPP__

#include <stddef.h>

#include "NoCopy.hpp"

class FileBuffer
{
public:
    explicit FileBuffer( const char* fn );
    ~FileBuffer();

    NoCopy( FileBuffer );

    [[nodiscard]] const char* data() const { return m_buffer; }
    [[nodiscard]] size_t size() const { return m_size; }

private:
    char* m_buffer;
    size_t m_size;
};

#endif
