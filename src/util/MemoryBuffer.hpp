#pragma once

#include <string>
#include <vector>

#include "DataBuffer.hpp"
#include "NoCopy.hpp"

class MemoryBuffer : public DataBuffer
{
public:
    struct BorrowTag {};
    static constexpr BorrowTag Borrow {};

    MemoryBuffer() = default;
    explicit MemoryBuffer( std::vector<char>&& buf );
    explicit MemoryBuffer( int fd );        // owning - close fd
    MemoryBuffer( int fd, BorrowTag );      // non-owning - leaves fd open

    [[nodiscard]] std::string AsString() const;

    NoCopy( MemoryBuffer );

private:
    void InitFromFd( int fd, bool owning );

    std::vector<char> m_buf;
};
