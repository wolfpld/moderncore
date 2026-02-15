#include <catch2/catch_all.hpp>
#include <src/util/DataBuffer.hpp>
#include <src/util/MemoryBuffer.hpp>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <vector>

TEST_CASE( "MemoryBuffer functionality", "[memorybuffer][buffer]" )
{
    SECTION( "Default constructor" )
    {
        MemoryBuffer memBuffer;

        REQUIRE( memBuffer.data() == nullptr );
        REQUIRE( memBuffer.size() == 0 );
    }

    SECTION( "Constructor with std::vector<char>&& (move semantics)" )
    {
        std::vector<char> testData = { 'H', 'e', 'l', 'l', 'o' };

        MemoryBuffer memBuffer( std::move( testData ) );

        REQUIRE( memBuffer.data() != nullptr );
        REQUIRE( memBuffer.size() == 5 );

        const char* dataPtr = memBuffer.data();
        const char* expected = "Hello";
        REQUIRE( memcmp( dataPtr, expected, 5 ) == 0 );

        REQUIRE( testData.empty() );
    }

    SECTION( "Constructor with empty vector" )
    {
        std::vector<char> emptyData;

        MemoryBuffer memBuffer( std::move( emptyData ) );

        REQUIRE( memBuffer.size() == 0 );
        REQUIRE( memBuffer.data() == nullptr );
    }

    SECTION( "AsString method returns correct string" )
    {
        std::vector<char> testData = { 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!' };

        MemoryBuffer memBuffer( std::move( testData ) );

        std::string result = memBuffer.AsString();

        const char* expected = "Hello World!";
        REQUIRE( result == expected );
        REQUIRE( result.length() == strlen( expected ) );
    }

    SECTION( "AsString with empty buffer" )
    {
        std::vector<char> emptyData;

        MemoryBuffer memBuffer( std::move( emptyData ) );

        std::string result = memBuffer.AsString();

        REQUIRE( result.empty() );
        REQUIRE( result.length() == 0 );
    }

    SECTION( "Constructor with file descriptor - reads file content" )
    {
        const char* testContent = "Test content from file descriptor";
        const char* tempFile = "/tmp/mcore_test_fd_XXXXXX";
        char tempPath[256];
        strncpy( tempPath, tempFile, sizeof( tempPath ) );

        int fd = mkstemp( tempPath );
        REQUIRE( fd >= 0 );

        write( fd, testContent, strlen( testContent ) );
        lseek( fd, 0, SEEK_SET );

        MemoryBuffer memBuffer( fd );

        REQUIRE( memBuffer.data() != nullptr );
        REQUIRE( memBuffer.size() == strlen( testContent ) );
        REQUIRE( memcmp( memBuffer.data(), testContent, strlen( testContent ) ) == 0 );

        unlink( tempPath );
    }

    SECTION( "Constructor with invalid file descriptor" )
    {
        MemoryBuffer memBuffer( -1 );

        REQUIRE( memBuffer.data() == nullptr );
        REQUIRE( memBuffer.size() == 0 );
    }

    SECTION( "Constructor with empty file descriptor" )
    {
        const char* tempFile = "/tmp/mcore_test_empty_XXXXXX";
        char tempPath[256];
        strncpy( tempPath, tempFile, sizeof( tempPath ) );

        int fd = mkstemp( tempPath );
        REQUIRE( fd >= 0 );

        MemoryBuffer memBuffer( fd );

        REQUIRE( memBuffer.data() == nullptr );
        REQUIRE( memBuffer.size() == 0 );

        unlink( tempPath );
    }

    SECTION( "Constructor with large file content" )
    {
        const char* tempFile = "/tmp/mcore_test_large_XXXXXX";
        char tempPath[256];
        strncpy( tempPath, tempFile, sizeof( tempPath ) );

        int fd = mkstemp( tempPath );
        REQUIRE( fd >= 0 );

        std::vector<char> largeContent( 100000, 'X' );
        write( fd, largeContent.data(), largeContent.size() );
        lseek( fd, 0, SEEK_SET );

        MemoryBuffer memBuffer( fd );

        REQUIRE( memBuffer.data() != nullptr );
        REQUIRE( memBuffer.size() == 100000 );
        REQUIRE( memcmp( memBuffer.data(), largeContent.data(), largeContent.size() ) == 0 );

        unlink( tempPath );
    }

    SECTION( "AsString with binary data containing null bytes" )
    {
        std::vector<char> binaryData = { 'H', 'i', '\0', 'T', 'h', 'e', 'r', 'e' };

        MemoryBuffer memBuffer( std::move( binaryData ) );

        std::string result = memBuffer.AsString();

        REQUIRE( result.size() == 8 );
        REQUIRE( result[0] == 'H' );
        REQUIRE( result[1] == 'i' );
        REQUIRE( result[2] == '\0' );
        REQUIRE( result[7] == 'e' );
    }
}
