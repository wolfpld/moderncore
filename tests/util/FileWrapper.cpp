#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/FileWrapper.hpp>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

TEST_CASE( "FileWrapper constructor opens file", "[filewrapper][ctor]" )
{
    SECTION( "Open existing file for reading" )
    {
        const char* content = "Hello, World!";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );

        REQUIRE( fw );
    }

    SECTION( "Create new file for writing" )
    {
        TempDir tempDir = TempDir::create();
        std::string path = tempDir.filePath( "newfile.txt" );

        {
            FileWrapper fw( path.c_str(), "wb" );
            REQUIRE( fw );
        }

        struct stat buf;
        REQUIRE( stat( path.c_str(), &buf ) == 0 );
    }

    SECTION( "Open non-existent file fails" )
    {
        FileWrapper fw( "/nonexistent/path/to/file.txt", "rb" );
        REQUIRE( !fw );
    }

    SECTION( "Open with append mode" )
    {
        const char* content = "Initial content";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "ab" );
        REQUIRE( fw );
    }

    SECTION( "Open empty file" )
    {
        auto tempFile = TempFile::createEmpty();

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );
    }
}

TEST_CASE( "FileWrapper destructor closes file", "[filewrapper][dtor]" )
{
    SECTION( "Destructor closes file properly" )
    {
        const char* content = "Test content";
        auto tempFile = TempFile::create( content, strlen( content ) );

        {
            FileWrapper fw( tempFile.path(), "rb" );
            REQUIRE( fw );
        }

        FILE* f = fopen( tempFile.path(), "rb" );
        REQUIRE( f != nullptr );
        fclose( f );
    }

    SECTION( "Destructor handles invalid file handle" )
    {
        FileWrapper* fw = new FileWrapper( "/nonexistent", "rb" );
        REQUIRE( !*fw );
        delete fw;
    }
}

TEST_CASE( "FileWrapper Read functionality", "[filewrapper][read]" )
{
    SECTION( "Read exact file size" )
    {
        const char* content = "Hello, World!";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        char buffer[20] = {};
        bool result = fw.Read( buffer, strlen( content ) );

        REQUIRE( result );
        REQUIRE( memcmp( buffer, content, strlen( content ) ) == 0 );
    }

    SECTION( "Read partial content" )
    {
        const char* content = "Hello, World!";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        char buffer[6] = {};
        bool result = fw.Read( buffer, 5 );

        REQUIRE( result );
        REQUIRE( memcmp( buffer, "Hello", 5 ) == 0 );
    }

    SECTION( "Read in multiple chunks" )
    {
        const char* content = "ABCDEFGHIJ";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        char buffer1[4] = {};
        char buffer2[4] = {};
        char buffer3[4] = {};

        REQUIRE( fw.Read( buffer1, 3 ) );
        REQUIRE( fw.Read( buffer2, 3 ) );
        REQUIRE( fw.Read( buffer3, 3 ) );

        REQUIRE( memcmp( buffer1, "ABC", 3 ) == 0 );
        REQUIRE( memcmp( buffer2, "DEF", 3 ) == 0 );
        REQUIRE( memcmp( buffer3, "GHI", 3 ) == 0 );
    }

    SECTION( "Read returns false at EOF" )
    {
        const char* content = "Short";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        char buffer[10];
        REQUIRE( fw.Read( buffer, 5 ) );

        bool result = fw.Read( buffer, 5 );
        REQUIRE( !result );
    }

    SECTION( "Read from empty file returns false" )
    {
        auto tempFile = TempFile::createEmpty();

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        char buffer[10];
        bool result = fw.Read( buffer, 1 );
        REQUIRE( !result );
    }

    SECTION( "Read large file" )
    {
        const size_t size = 100000;
        std::vector<char> data = BinaryPattern::sequence( size );
        auto tempFile = TempFile::create( data.data(), size );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        std::vector<char> buffer( size );
        bool result = fw.Read( buffer.data(), size );

        REQUIRE( result );
        REQUIRE( memcmp( buffer.data(), data.data(), size ) == 0 );
    }

    SECTION( "Read binary data with null bytes" )
    {
        char content[] = { 'H', 'e', '\0', 'l', 'l', 'o' };
        auto tempFile = TempFile::create( content, sizeof( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        char buffer[6] = {};
        bool result = fw.Read( buffer, sizeof( content ) );

        REQUIRE( result );
        REQUIRE( memcmp( buffer, content, sizeof( content ) ) == 0 );
    }
}

TEST_CASE( "FileWrapper Release transfers ownership", "[filewrapper][release]" )
{
    SECTION( "Release prevents fclose on destructor" )
    {
        const char* content = "Test content";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FILE* rawFile = nullptr;
        {
            FileWrapper fw( tempFile.path(), "rb" );
            REQUIRE( fw );

            rawFile = fw;
            fw.Release();
        }

        REQUIRE( rawFile != nullptr );
        char buffer[20] = {};
        REQUIRE( fread( buffer, 1, strlen( content ), rawFile ) == strlen( content ) );
        fclose( rawFile );
    }

    SECTION( "Released file can still be used" )
    {
        const char* content = "Hello";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        FILE* rawFile = fw;
        fw.Release();

        char buffer[10] = {};
        REQUIRE( fread( buffer, 1, 5, rawFile ) == 5 );
        REQUIRE( memcmp( buffer, "Hello", 5 ) == 0 );

        fclose( rawFile );
    }

    SECTION( "File still exists after Release and manual close" )
    {
        const char* content = "Persistent";
        auto tempFile = TempFile::create( content, strlen( content ) );

        {
            FileWrapper fw( tempFile.path(), "rb" );
            FILE* rawFile = fw;
            fw.Release();
            fclose( rawFile );
        }

        REQUIRE( tempFile.exists() );
    }
}

TEST_CASE( "FileWrapper operator bool validates state", "[filewrapper][bool]" )
{
    SECTION( "Valid file returns true" )
    {
        auto tempFile = TempFile::createEmpty();

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( static_cast<bool>( fw ) );
    }

    SECTION( "Invalid file returns false" )
    {
        FileWrapper fw( "/nonexistent/path/file.txt", "rb" );
        REQUIRE( !static_cast<bool>( fw ) );
    }

    SECTION( "Explicit bool conversion with true case" )
    {
        auto tempFile = TempFile::createEmpty();

        FileWrapper fw( tempFile.path(), "rb" );
        bool result = fw.operator bool();
        REQUIRE( result );
    }

    SECTION( "Explicit bool conversion with false case" )
    {
        FileWrapper fw( "/nonexistent", "rb" );
        bool result = fw.operator bool();
        REQUIRE( !result );
    }
}

TEST_CASE( "FileWrapper FILE* conversion", "[filewrapper][conversion]" )
{
    SECTION( "Convert to FILE* for standard IO" )
    {
        const char* content = "Test content";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        REQUIRE( fw );

        FILE* file = fw;
        REQUIRE( file != nullptr );

        char buffer[20] = {};
        REQUIRE( fread( buffer, 1, strlen( content ), file ) == strlen( content ) );
        REQUIRE( memcmp( buffer, content, strlen( content ) ) == 0 );
    }

    SECTION( "Use with fseek and ftell" )
    {
        const char* content = "ABCDEFGHIJ";
        auto tempFile = TempFile::create( content, strlen( content ) );

        FileWrapper fw( tempFile.path(), "rb" );
        FILE* file = fw;

        fseek( file, 5, SEEK_SET );
        REQUIRE( ftell( file ) == 5 );

        char buffer[3] = {};
        fread( buffer, 1, 2, file );
        REQUIRE( memcmp( buffer, "FG", 2 ) == 0 );
    }
}

TEST_CASE( "FileWrapper write operations", "[filewrapper][write]" )
{
    SECTION( "Write to file" )
    {
        TempDir tempDir = TempDir::create();
        std::string path = tempDir.filePath( "write_test.txt" );

        {
            FileWrapper fw( path.c_str(), "wb" );
            REQUIRE( fw );

            FILE* file = fw;
            const char* content = "Written content";
            fwrite( content, 1, strlen( content ), file );
        }

        FileWrapper fwRead( path.c_str(), "rb" );
        REQUIRE( fwRead );

        char buffer[20] = {};
        fwRead.Read( buffer, strlen( "Written content" ) );
        REQUIRE( memcmp( buffer, "Written content", strlen( "Written content" ) ) == 0 );
    }
}