#pragma once

#include <string>
#include <vector>

#include "DataBuffer.hpp"
#include "NoCopy.hpp"

class MemoryBuffer : public DataBuffer
{
public:
    MemoryBuffer() = default;
    explicit MemoryBuffer( std::vector<char>&& buf );
    explicit MemoryBuffer( int fd );

    [[nodiscard]] std::string AsString() const;

    NoCopy( MemoryBuffer );

private:
    std::vector<char> m_buf;
};
