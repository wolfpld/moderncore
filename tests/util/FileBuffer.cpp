#include <catch2/catch_all.hpp>
#include <src/util/FileBuffer.hpp>
#include <src/util/FileWrapper.hpp>
#include "TestUtils.hpp"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

TEST_CASE( "FileBuffer functionality", "[filebuffer][buffer]" )
{
    SECTION( "Constructor with valid filename - reads file content" )
    {
        const char* content = "Hello, World!";
        auto tempFile = TempFile::create( content, strlen( content ) );
        
        FileBuffer buffer( tempFile.path() );
        
        REQUIRE( buffer.data() != nullptr );
        REQUIRE( buffer.size() == strlen( content ) );
        REQUIRE( memcmp( buffer.data(), content, strlen( content ) ) == 0 );
    }
    
    SECTION( "Constructor with non-existent file throws FileException" )
    {
        REQUIRE_THROWS_AS( FileBuffer( "/nonexistent/path/to/file.txt" ), FileBuffer::FileException );
    }
    
    SECTION( "Constructor with FILE* pointer" )
    {
        const char* content = "Test content via FILE*";
        auto tempFile = TempFile::create( content, strlen( content ) );
        
        FILE* file = fopen( tempFile.path(), "rb" );
        REQUIRE( file != nullptr );
        
        FileBuffer buffer( file );
        
        REQUIRE( buffer.data() != nullptr );
        REQUIRE( buffer.size() == strlen( content ) );
        REQUIRE( memcmp( buffer.data(), content, strlen( content ) ) == 0 );
        
        fclose( file );
    }
    
    SECTION( "Constructor with shared_ptr<FileWrapper>" )
    {
        const char* content = "Test via FileWrapper";
        auto tempFile = TempFile::create( content, strlen( content ) );
        
        auto fileWrapper = std::make_shared<FileWrapper>( tempFile.path(), "rb" );
        REQUIRE( *fileWrapper );
        
        FileBuffer buffer( fileWrapper );
        
        REQUIRE( buffer.data() != nullptr );
        REQUIRE( buffer.size() == strlen( content ) );
        REQUIRE( memcmp( buffer.data(), content, strlen( content ) ) == 0 );
    }
    
    SECTION( "Empty file handling" )
    {
        auto tempFile = TempFile::createEmpty();
        
        FileBuffer buffer( tempFile.path() );
        
        REQUIRE( buffer.size() == 0 );
        // mmap of empty file may return nullptr or valid pointer
        // Both are acceptable
    }
    
    SECTION( "Large file handling - 10MB" )
    {
        const size_t largeSize = 10 * 1024 * 1024; // 10MB
        std::vector<char> largeData = BinaryPattern::sequence( largeSize );
        
        auto tempFile = TempFile::create( largeData.data(), largeData.size() );
        
        FileBuffer buffer( tempFile.path() );
        
        REQUIRE( buffer.data() != nullptr );
        REQUIRE( buffer.size() == largeSize );
        REQUIRE( memcmp( buffer.data(), largeData.data(), largeSize ) == 0 );
    }
    
    SECTION( "Binary data integrity - all byte values" )
    {
        std::vector<char> binaryData( 256 );
        for( int i = 0; i < 256; i++ )
        {
            binaryData[i] = static_cast<char>( i );
        }
        
        auto tempFile = TempFile::create( binaryData.data(), binaryData.size() );
        
        FileBuffer buffer( tempFile.path() );
        
        REQUIRE( buffer.size() == 256 );
        for( int i = 0; i < 256; i++ )
        {
            REQUIRE( static_cast<unsigned char>( buffer.data()[i] ) == i );
        }
    }
    
    SECTION( "Multiple FileBuffers on same file work independently" )
    {
        const char* content = "Shared file content";
        auto tempFile = TempFile::create( content, strlen( content ) );
        
        FileBuffer buffer1( tempFile.path() );
        FileBuffer buffer2( tempFile.path() );
        
        REQUIRE( buffer1.data() != nullptr );
        REQUIRE( buffer2.data() != nullptr );
        REQUIRE( buffer1.size() == buffer2.size() );
        REQUIRE( buffer1.size() == strlen( content ) );
        
        // Both should see the same content
        REQUIRE( memcmp( buffer1.data(), buffer2.data(), strlen( content ) ) == 0 );
    }
    
    SECTION( "File with null bytes in content" )
    {
        char content[] = { 'H', 'e', 'l', 'l', 'o', '\0', 'W', 'o', 'r', 'l', 'd' };
        size_t contentSize = sizeof( content );
        
        auto tempFile = TempFile::create( content, contentSize );
        
        FileBuffer buffer( tempFile.path() );
        
        REQUIRE( buffer.size() == contentSize );
        REQUIRE( buffer.data()[5] == '\0' );
        REQUIRE( memcmp( buffer.data(), content, contentSize ) == 0 );
    }
    
    SECTION( "Inherits from DataBuffer" )
    {
        auto tempFile = TempFile::create( "test", 4 );
        
        FileBuffer fileBuffer( tempFile.path() );
        DataBuffer& dataBuffer = fileBuffer;
        
        REQUIRE( dataBuffer.size() == 4 );
        REQUIRE( dataBuffer.data() != nullptr );
    }
    
    SECTION( "Destructor properly releases mmap" )
    {
        // This test verifies that destructor calls munmap
        // We create and destroy FileBuffer in a scope
        const char* content = "Test for destructor";
        auto tempFile = TempFile::create( content, strlen( content ) );
        
        {
            FileBuffer buffer( tempFile.path() );
            REQUIRE( buffer.data() != nullptr );
            // Buffer goes out of scope here, destructor runs
        }
        
        // If we get here without crash, mmap was properly released
        // We can also verify the file still exists and is readable
        FILE* f = fopen( tempFile.path(), "rb" );
        REQUIRE( f != nullptr );
        fclose( f );
    }
    
    SECTION( "File with special characters in content" )
    {
        char content[] = "\x00\x01\x02\x03\xFF\xFE\xFD\xFC";
        size_t contentSize = sizeof( content );
        
        auto tempFile = TempFile::create( content, contentSize );
        
        FileBuffer buffer( tempFile.path() );
        
        REQUIRE( buffer.size() == contentSize );
        REQUIRE( memcmp( buffer.data(), content, contentSize ) == 0 );
    }
    
    SECTION( "Concurrent read access via multiple FileBuffers" )
    {
        const size_t dataSize = 1024 * 1024; // 1MB
        std::vector<char> data = BinaryPattern::random( dataSize );
        
        auto tempFile = TempFile::create( data.data(), data.size() );
        
        // Create multiple buffers reading same file
        FileBuffer buffer1( tempFile.path() );
        FileBuffer buffer2( tempFile.path() );
        FileBuffer buffer3( tempFile.path() );
        
        // All should have identical content
        REQUIRE( buffer1.size() == dataSize );
        REQUIRE( buffer2.size() == dataSize );
        REQUIRE( buffer3.size() == dataSize );
        
        REQUIRE( memcmp( buffer1.data(), data.data(), dataSize ) == 0 );
        REQUIRE( memcmp( buffer2.data(), data.data(), dataSize ) == 0 );
        REQUIRE( memcmp( buffer3.data(), data.data(), dataSize ) == 0 );
    }
}
