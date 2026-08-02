#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <lz4.h>
#include <src/util/EmbedData.hpp>
#include <stdint.h>
#include <string.h>
#include <vector>

namespace
{

// Compress the given data with LZ4, returning the compressed blob
std::vector<uint8_t> Compress( const char* data, size_t size )
{
    const auto compressedSize = LZ4_compressBound( (int)size );
    std::vector<uint8_t> compressed( compressedSize );

    const auto written = LZ4_compress_default( data, (char*)compressed.data(), (int)size, compressedSize );
    REQUIRE( written > 0 );

    compressed.resize( written );
    return compressed;
}

}

TEST_CASE( "EmbedData decompression", "[embeddata]" )
{
    SECTION( "Round trip through LZ4" )
    {
        const char* content = "Embedded data test payload";
        const auto size = strlen( content );

        const auto compressed = Compress( content, size );

        EmbedData embed( size, compressed.size(), compressed.data() );

        REQUIRE( embed.size() == size );
        REQUIRE( embed.data() != nullptr );
        REQUIRE( memcmp( embed.data(), content, size ) == 0 );
    }

    SECTION( "Binary payload with null bytes" )
    {
        std::vector<char> content = { 'H', 'e', 'l', 'l', 'o', '\0', 'W', 'o', 'r', 'l', 'd', '\0', '\xFF', '\xFE' };
        const auto compressed = Compress( content.data(), content.size() );

        EmbedData embed( content.size(), compressed.size(), compressed.data() );

        REQUIRE( embed.size() == content.size() );
        REQUIRE( memcmp( embed.data(), content.data(), content.size() ) == 0 );
    }

    SECTION( "Large payload" )
    {
        auto content = BinaryPattern::random( 1024 * 1024 );
        const auto compressed = Compress( content.data(), content.size() );

        EmbedData embed( content.size(), compressed.size(), compressed.data() );

        REQUIRE( embed.size() == content.size() );
        REQUIRE( memcmp( embed.data(), content.data(), content.size() ) == 0 );
    }

    SECTION( "Inherits from DataBuffer" )
    {
        const char* content = "buffer inheritance";
        const auto size = strlen( content );
        const auto compressed = Compress( content, size );

        EmbedData embed( size, compressed.size(), compressed.data() );
        DataBuffer& ref = embed;

        REQUIRE( ref.size() == size );
        REQUIRE( ref.data() == embed.data() );
    }

    SECTION( "Corrupted compressed data aborts" )
    {
        const char* content = "corruption test payload";
        const auto size = strlen( content );
        auto compressed = Compress( content, size );

        // Corrupt the block-size header so decompression fails deterministically
        compressed[0] ^= 0xFF;

        REQUIRE_ABORTS( EmbedData( size, compressed.size(), compressed.data() ) );
    }

    SECTION( "Truncated compressed data aborts" )
    {
        const char* content = "truncated payload";
        const auto size = strlen( content );
        auto compressed = Compress( content, size );

        // A single byte cannot contain a valid LZ4 block header
        compressed.resize( 1 );

        REQUIRE_ABORTS( EmbedData( size, compressed.size(), compressed.data() ) );
    }
}
