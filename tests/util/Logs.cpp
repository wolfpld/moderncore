#include "TestUtils.hpp"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <src/util/Logs.hpp>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

class LogLevelGuard
{
public:
    LogLevelGuard() { m_original = GetLogLevel(); }
    ~LogLevelGuard() { SetLogLevel( m_original ); }

private:
    LogLevel m_original;
};

class LogSyncGuard
{
public:
    ~LogSyncGuard() { SetLogSynchronized( false ); }
};

class LogFileGuard
{
public:
    ~LogFileGuard()
    {
        if( m_enabled )
        {
            SetLogToFile( false );
        }
    }

    void Enable()
    {
        SetLogToFile( true );
        m_enabled = true;
    }

    void Disable()
    {
        SetLogToFile( false );
        m_enabled = false;
    }

private:
    bool m_enabled = false;
};

static std::string captureLogOutput( LogLevel level, const char* msg )
{
    OutputCapture capture;
    mclog( level, "%s", msg );
    return stripAnsi( capture.getOutput() );
}

TEST_CASE( "SetLogLevel and GetLogLevel", "[logs][level]" )
{
    LogLevelGuard levelGuard;

    SECTION( "Set and get each log level" )
    {
        auto level = GENERATE( LogLevel::Callstack, LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error, LogLevel::ErrorTrace, LogLevel::Fatal );
        SetLogLevel( level );
        REQUIRE( GetLogLevel() == level );
    }
}

TEST_CASE( "LogBlockBegin/End synchronize logging", "[logs][sync][block]" )
{
    SECTION( "Synchronized mode blocks other threads until LogBlockEnd" )
    {
        LogSyncGuard syncGuard;
        SetLogSynchronized( true );

        LogBlockBegin();

        std::atomic<bool> started{ false };
        std::atomic<bool> finished{ false };
        std::thread worker( [&] {
            started = true;
            mclog( LogLevel::Info, "worker message" );
            finished = true;
        } );

        while( !started.load() ) {}

        REQUIRE( !finished.load() );

        LogBlockEnd();
        worker.join();
        REQUIRE( finished.load() );
    }

    SECTION( "Unsynchronized mode does not block other threads" )
    {
        LogSyncGuard syncGuard;
        SetLogSynchronized( false );

        LogBlockBegin();
        std::atomic<bool> finished{ false };
        std::thread worker( [&] {
            mclog( LogLevel::Info, "worker message" );
            finished = true;
        } );
        worker.join();
        LogBlockEnd();

        REQUIRE( finished.load() );
    }

    SECTION( "Nested blocks with synchronized mode" )
    {
        LogSyncGuard syncGuard;
        SetLogSynchronized( true );

        LogBlockBegin();
        LogBlockBegin();
        LogBlockEnd();
        LogBlockEnd();
    }
}

TEST_CASE( "MCoreLogMessage filters by log level", "[logs][filter]" )
{
    LogLevelGuard levelGuard;

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
}

TEST_CASE( "MCoreLogMessage format handling", "[logs][format]" )
{
    LogLevelGuard levelGuard;
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
}

TEST_CASE( "MCoreLogMessage level indicators", "[logs][indicator]" )
{
    LogLevelGuard levelGuard;
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
}

TEST_CASE( "MCoreLogMessage source location", "[logs][location]" )
{
    LogLevelGuard levelGuard;
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

    SECTION( "Short file name is right-aligned in the source column" )
    {
        SetLogLevel( LogLevel::Info );

        OutputCapture capture;
#line 42 "short"
        mclog( LogLevel::Info, "hello" );
        auto output = stripAnsi( capture.getOutput() );

        REQUIRE( output.find( "short" ) != std::string::npos );
        REQUIRE( output.find( ":42" ) != std::string::npos );
        REQUIRE( output.find( "hello" ) != std::string::npos );
    }
}

TEST_CASE( "SetLogToFile writes to file", "[logs][file]" )
{
    LogLevelGuard levelGuard;
    SetLogLevel( LogLevel::Debug );

    CwdGuard cwdGuard;

    TempDir tempDir = TempDir::create();
    chdir( tempDir.path() );

    SECTION( "Enable file logging creates file" )
    {
        LogFileGuard fileGuard;
        fileGuard.Enable();

        struct stat buf;
        REQUIRE( stat( "mcore.log", &buf ) == 0 );
    }

    SECTION( "Log messages written to file" )
    {
        LogFileGuard fileGuard;
        fileGuard.Enable();

        mclog( LogLevel::Info, "file test message" );

        fileGuard.Disable();

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
        LogFileGuard fileGuard;
        fileGuard.Enable();

        mclog( LogLevel::Info, "message one" );
        mclog( LogLevel::Warning, "message two" );
        mclog( LogLevel::Error, "message three" );

        fileGuard.Disable();

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
        LogFileGuard fileGuard;
        fileGuard.Enable();
        mclog( LogLevel::Info, "first batch" );
        fileGuard.Disable();

        fileGuard.Enable();
        mclog( LogLevel::Info, "second batch" );
        fileGuard.Disable();

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
}

TEST_CASE( "MCoreLogMessage invalid level aborts", "[logs][panic]" )
{
    LogLevelGuard levelGuard;
    SetLogLevel( LogLevel::Info );
    REQUIRE_ABORTS( mclog( (LogLevel)999, "invalid level" ) );
}

TEST_CASE( "Log levels can be changed multiple times", "[logs][state]" )
{
    LogLevelGuard levelGuard;

    SetLogLevel( LogLevel::Debug );
    REQUIRE( GetLogLevel() == LogLevel::Debug );

    SetLogLevel( LogLevel::Warning );
    REQUIRE( GetLogLevel() == LogLevel::Warning );

    SetLogLevel( LogLevel::Error );
    REQUIRE( GetLogLevel() == LogLevel::Error );

    SetLogLevel( LogLevel::Info );
    REQUIRE( GetLogLevel() == LogLevel::Info );
}