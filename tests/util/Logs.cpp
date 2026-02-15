#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Logs.hpp>
#include <sys/stat.h>
#include <unistd.h>

static std::string captureLogOutput( LogLevel level, const char* msg )
{
    OutputCapture capture;
    mclog( level, "%s", msg );
    return stripAnsi( capture.getOutput() );
}

TEST_CASE( "SetLogLevel and GetLogLevel", "[logs][level]" )
{
    LogLevel originalLevel = GetLogLevel();

    SECTION( "Set and get each log level" )
    {
        auto level = GENERATE( LogLevel::Callstack, LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error, LogLevel::ErrorTrace, LogLevel::Fatal );
        SetLogLevel( level );
        REQUIRE( GetLogLevel() == level );
    }

    SetLogLevel( originalLevel );
}

TEST_CASE( "SetLogSynchronized toggles synchronized mode", "[logs][sync]" )
{
    bool originalSync = false;

    SECTION( "Enable synchronized mode" )
    {
        SetLogSynchronized( true );
    }

    SECTION( "Disable synchronized mode" )
    {
        SetLogSynchronized( false );
    }

    SetLogSynchronized( originalSync );
}

TEST_CASE( "LogBlockBegin and LogBlockEnd work correctly", "[logs][block]" )
{
    SECTION( "Block with synchronized mode enabled" )
    {
        SetLogSynchronized( true );
        LogBlockBegin();
        LogBlockEnd();
        SetLogSynchronized( false );
    }

    SECTION( "Block with synchronized mode disabled" )
    {
        SetLogSynchronized( false );
        LogBlockBegin();
        LogBlockEnd();
    }

    SECTION( "Nested blocks" )
    {
        SetLogSynchronized( true );
        LogBlockBegin();
        LogBlockBegin();
        LogBlockEnd();
        LogBlockEnd();
        SetLogSynchronized( false );
    }
}

TEST_CASE( "MCoreLogMessage filters by log level", "[logs][filter]" )
{
    LogLevel originalLevel = GetLogLevel();

    SECTION( "Messages below current level are filtered" )
    {
        SetLogLevel( LogLevel::Warning );

        std::string debugOutput = captureLogOutput( LogLevel::Debug, "debug message" );
        std::string infoOutput = captureLogOutput( LogLevel::Info, "info message" );

        REQUIRE( debugOutput.empty() );
        REQUIRE( infoOutput.empty() );
    }

    SECTION( "Messages at or above current level are output" )
    {
        SetLogLevel( LogLevel::Warning );

        std::string warnOutput = captureLogOutput( LogLevel::Warning, "warning message" );
        std::string errorOutput = captureLogOutput( LogLevel::Error, "error message" );

        REQUIRE( !warnOutput.empty() );
        REQUIRE( warnOutput.find( "warning message" ) != std::string::npos );

        REQUIRE( !errorOutput.empty() );
        REQUIRE( errorOutput.find( "error message" ) != std::string::npos );
    }

    SECTION( "Debug level shows all messages" )
    {
        SetLogLevel( LogLevel::Debug );

        auto level = GENERATE( LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error, LogLevel::Fatal );
        std::string output = captureLogOutput( level, "test message" );

        REQUIRE( !output.empty() );
        REQUIRE( output.find( "test message" ) != std::string::npos );
    }

    SECTION( "Fatal level shows only fatal" )
    {
        SetLogLevel( LogLevel::Fatal );

        std::string errorOutput = captureLogOutput( LogLevel::Error, "error message" );
        std::string fatalOutput = captureLogOutput( LogLevel::Fatal, "fatal message" );

        REQUIRE( errorOutput.empty() );
        REQUIRE( !fatalOutput.empty() );
        REQUIRE( fatalOutput.find( "fatal message" ) != std::string::npos );
    }

    SECTION( "Callstack level is always output" )
    {
        SetLogLevel( LogLevel::Fatal );

        std::string callstackOutput = captureLogOutput( LogLevel::Callstack, "callstack message" );

        REQUIRE( !callstackOutput.empty() );
        REQUIRE( callstackOutput.find( "callstack message" ) != std::string::npos );
    }

    SetLogLevel( originalLevel );
}

TEST_CASE( "MCoreLogMessage format handling", "[logs][format]" )
{
    LogLevel originalLevel = GetLogLevel();
    SetLogLevel( LogLevel::Debug );

    SECTION( "Simple string message" )
    {
        std::string output = captureLogOutput( LogLevel::Info, "hello world" );
        REQUIRE( !output.empty() );
        REQUIRE( output.find( "hello world" ) != std::string::npos );
    }

    SECTION( "Message with format specifiers" )
    {
        OutputCapture capture;
        mclog( LogLevel::Info, "value: %d, string: %s", 42, "test" );
        std::string output = stripAnsi( capture.getOutput() );

        REQUIRE( output.find( "42" ) != std::string::npos );
        REQUIRE( output.find( "test" ) != std::string::npos );
    }

    SECTION( "Empty message" )
    {
        std::string output = captureLogOutput( LogLevel::Info, "" );
        REQUIRE( !output.empty() );
    }

    SECTION( "Message with special characters" )
    {
        std::string output = captureLogOutput( LogLevel::Info, "special: \t\n\\\"'" );
        REQUIRE( !output.empty() );
    }

    SECTION( "Long message" )
    {
        std::string longMsg( 500, 'X' );
        std::string output = captureLogOutput( LogLevel::Info, longMsg.c_str() );
        REQUIRE( !output.empty() );
        REQUIRE( output.find( longMsg ) != std::string::npos );
    }

    SetLogLevel( originalLevel );
}

TEST_CASE( "MCoreLogMessage level indicators", "[logs][indicator]" )
{
    LogLevel originalLevel = GetLogLevel();
    SetLogLevel( LogLevel::Debug );

    SECTION( "Debug level indicator" )
    {
        std::string output = captureLogOutput( LogLevel::Debug, "msg" );
        REQUIRE( output.find( "[DEBUG]" ) != std::string::npos );
    }

    SECTION( "Info level indicator" )
    {
        std::string output = captureLogOutput( LogLevel::Info, "msg" );
        REQUIRE( output.find( "[INFO]" ) != std::string::npos );
    }

    SECTION( "Warning level indicator" )
    {
        std::string output = captureLogOutput( LogLevel::Warning, "msg" );
        REQUIRE( output.find( "[WARN]" ) != std::string::npos );
    }

    SECTION( "Error level indicator" )
    {
        std::string output = captureLogOutput( LogLevel::Error, "msg" );
        REQUIRE( output.find( "[ERROR]" ) != std::string::npos );
    }

    SECTION( "Fatal level indicator" )
    {
        std::string output = captureLogOutput( LogLevel::Fatal, "msg" );
        REQUIRE( output.find( "[FATAL]" ) != std::string::npos );
    }

    SECTION( "Callstack level indicator" )
    {
        std::string output = captureLogOutput( LogLevel::Callstack, "msg" );
        REQUIRE( output.find( "[STACK]" ) != std::string::npos );
    }

    SetLogLevel( originalLevel );
}

TEST_CASE( "MCoreLogMessage source location", "[logs][location]" )
{
    LogLevel originalLevel = GetLogLevel();
    SetLogLevel( LogLevel::Debug );

    SECTION( "Output contains source file information" )
    {
        std::string output = captureLogOutput( LogLevel::Info, "test" );
        REQUIRE( output.find( ".cpp" ) != std::string::npos );
    }

    SECTION( "Output contains line number" )
    {
        std::string output = captureLogOutput( LogLevel::Info, "test" );
        bool hasLineNumber = false;
        for( char c : output )
        {
            if( c >= '0' && c <= '9' )
            {
                hasLineNumber = true;
                break;
            }
        }
        REQUIRE( hasLineNumber );
    }

    SetLogLevel( originalLevel );
}

TEST_CASE( "SetLogToFile writes to file", "[logs][file]" )
{
    LogLevel originalLevel = GetLogLevel();
    SetLogLevel( LogLevel::Debug );

    char originalCwd[PATH_MAX];
    getcwd( originalCwd, sizeof( originalCwd ) );

    TempDir tempDir = TempDir::create();
    chdir( tempDir.path() );

    SECTION( "Enable file logging creates file" )
    {
        SetLogToFile( true );

        struct stat buf;
        REQUIRE( stat( "mcore.log", &buf ) == 0 );

        SetLogToFile( false );
    }

    SECTION( "Log messages written to file" )
    {
        SetLogToFile( true );

        mclog( LogLevel::Info, "file test message" );

        SetLogToFile( false );

        FILE* f = fopen( "mcore.log", "r" );
        REQUIRE( f != nullptr );

        char buffer[256];
        std::string content;
        while( fgets( buffer, sizeof( buffer ), f ) )
        {
            content += buffer;
        }
        fclose( f );

        REQUIRE( content.find( "file test message" ) != std::string::npos );
    }

    SECTION( "Multiple messages logged to file" )
    {
        SetLogToFile( true );

        mclog( LogLevel::Info, "message one" );
        mclog( LogLevel::Warning, "message two" );
        mclog( LogLevel::Error, "message three" );

        SetLogToFile( false );

        FILE* f = fopen( "mcore.log", "r" );
        REQUIRE( f != nullptr );

        char buffer[1024];
        std::string content;
        while( fgets( buffer, sizeof( buffer ), f ) )
        {
            content += buffer;
        }
        fclose( f );

        REQUIRE( content.find( "message one" ) != std::string::npos );
        REQUIRE( content.find( "message two" ) != std::string::npos );
        REQUIRE( content.find( "message three" ) != std::string::npos );
    }

    SECTION( "File logging can be re-enabled" )
    {
        SetLogToFile( true );
        mclog( LogLevel::Info, "first batch" );
        SetLogToFile( false );

        SetLogToFile( true );
        mclog( LogLevel::Info, "second batch" );
        SetLogToFile( false );

        FILE* f = fopen( "mcore.log", "r" );
        REQUIRE( f != nullptr );

        char buffer[256];
        std::string content;
        while( fgets( buffer, sizeof( buffer ), f ) )
        {
            content += buffer;
        }
        fclose( f );

        REQUIRE( content.find( "first batch" ) == std::string::npos );
        REQUIRE( content.find( "second batch" ) != std::string::npos );
    }

    chdir( originalCwd );
    SetLogLevel( originalLevel );
}

TEST_CASE( "Log levels can be changed multiple times", "[logs][state]" )
{
    LogLevel originalLevel = GetLogLevel();

    SetLogLevel( LogLevel::Debug );
    REQUIRE( GetLogLevel() == LogLevel::Debug );

    SetLogLevel( LogLevel::Warning );
    REQUIRE( GetLogLevel() == LogLevel::Warning );

    SetLogLevel( LogLevel::Error );
    REQUIRE( GetLogLevel() == LogLevel::Error );

    SetLogLevel( LogLevel::Info );
    REQUIRE( GetLogLevel() == LogLevel::Info );

    SetLogLevel( originalLevel );
}