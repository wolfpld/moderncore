#ifndef __FILEBUFFER_HPP__
#define __FILEBUFFER_HPP__

#include <stddef.h>

class FileBuffer
{
public:
    explicit FileBuffer( const char* fn );
    ~FileBuffer();

    [[nodiscard]] const char* Data() const { return m_buffer; }
    [[nodiscard]] size_t Size() const { return m_size; }

private:
    char* m_buffer;
    size_t m_size;
};

#endif
