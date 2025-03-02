#pragma once

#include <vector>

#include "DataBuffer.hpp"
#include "NoCopy.hpp"

class MemoryBuffer : public DataBuffer
{
public:
    explicit MemoryBuffer( std::vector<char>&& buf );

    NoCopy( MemoryBuffer );

private:
    std::vector<char> m_buf;
};
