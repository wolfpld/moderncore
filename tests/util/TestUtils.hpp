#pragma once

#include <catch2/catch_all.hpp>
#include <string>
#include <vector>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

// Helper class for managing temporary files
class TempFile
{
public:
    TempFile() = default;
    
    ~TempFile()
    {
        cleanup();
    }
    
    // No copy
    TempFile( const TempFile& ) = delete;
    TempFile& operator=( const TempFile& ) = delete;
    
    // Move allowed
    TempFile( TempFile&& other ) noexcept
        : m_path( std::move( other.m_path ) )
    {
        other.m_path.clear();
    }
    
    TempFile& operator=( TempFile&& other ) noexcept
    {
        if( this != &other )
        {
            cleanup();
            m_path = std::move( other.m_path );
            other.m_path.clear();
        }
        return *this;
    }
    
    // Create a temp file with optional content
    static TempFile create( const char* content = nullptr, size_t size = 0 )
    {
        TempFile tf;
        tf.m_path = tf.createTempFile( content, size );
        return tf;
    }
    
    // Create an empty temp file
    static TempFile createEmpty()
    {
        return create( nullptr, 0 );
    }
    
    // Get the file path
    const char* path() const { return m_path.c_str(); }
    const std::string& str() const { return m_path; }
    
    // Check if file exists
    bool exists() const
    {
        struct stat buf;
        return stat( m_path.c_str(), &buf ) == 0;
    }
    
    // Get file size
    size_t size() const
    {
        struct stat buf;
        if( stat( m_path.c_str(), &buf ) != 0 )
            return 0;
        return buf.st_size;
    }
    
    // Release ownership (won't delete on destruction)
    std::string release()
    {
        std::string result = std::move( m_path );
        m_path.clear();
        return result;
    }
    
private:
    void cleanup()
    {
        if( !m_path.empty() )
        {
            unlink( m_path.c_str() );
            m_path.clear();
        }
    }
    
    std::string createTempFile( const char* content, size_t contentSize )
    {
        char template_path[] = "/tmp/mcore_test_file_XXXXXX";
        int fd = mkstemp( template_path );
        REQUIRE( fd >= 0 );
        
        if( content && contentSize > 0 )
        {
            ssize_t written = write( fd, content, contentSize );
            REQUIRE( written == static_cast<ssize_t>( contentSize ) );
        }
        
        close( fd );
        return std::string( template_path );
    }
    
    std::string m_path;
};

// Helper class for managing temporary directories
class TempDir
{
public:
    TempDir() = default;
    
    ~TempDir()
    {
        cleanup();
    }
    
    // No copy
    TempDir( const TempDir& ) = delete;
    TempDir& operator=( const TempDir& ) = delete;
    
    // Move allowed
    TempDir( TempDir&& other ) noexcept
        : m_path( std::move( other.m_path ) )
    {
        other.m_path.clear();
    }
    
    TempDir& operator=( TempDir&& other ) noexcept
    {
        if( this != &other )
        {
            cleanup();
            m_path = std::move( other.m_path );
            other.m_path.clear();
        }
        return *this;
    }
    
    // Create a temp directory
    static TempDir create()
    {
        TempDir td;
        td.m_path = td.createTempDir();
        return td;
    }
    
    // Get the directory path
    const char* path() const { return m_path.c_str(); }
    const std::string& str() const { return m_path; }
    
    // Build path to file inside this directory
    std::string filePath( const char* filename ) const
    {
        return m_path + "/" + filename;
    }
    
    // Check if directory exists
    bool exists() const
    {
        struct stat buf;
        return stat( m_path.c_str(), &buf ) == 0 && S_ISDIR( buf.st_mode );
    }
    
    // Create a file inside this directory - returns path, caller manages cleanup
    std::string createFile( const char* name, const char* content = nullptr, size_t size = 0 )
    {
        std::string fullPath = m_path + "/" + name;
        
        int fd = open( fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644 );
        REQUIRE( fd >= 0 );
        
        if( content && size > 0 )
        {
            ssize_t written = write( fd, content, size );
            REQUIRE( written == static_cast<ssize_t>( size ) );
        }
        
        close( fd );
        
        return fullPath;
    }
    
    // Create a subdirectory
    void createSubdir( const char* name )
    {
        std::string path = m_path + "/" + name;
        int ret = mkdir( path.c_str(), 0755 );
        REQUIRE( ret == 0 );
    }
    
    // Release ownership (won't delete on destruction)
    std::string release()
    {
        std::string result = std::move( m_path );
        m_path.clear();
        return result;
    }
    
private:
    void cleanup()
    {
        if( !m_path.empty() )
        {
            removeDirRecursive( m_path.c_str() );
            m_path.clear();
        }
    }
    
    std::string createTempDir()
    {
        char template_path[] = "/tmp/mcore_test_dir_XXXXXX";
        char* result = mkdtemp( template_path );
        REQUIRE( result != nullptr );
        return std::string( result );
    }
    
    void removeDirRecursive( const char* path )
    {
        // Simple recursive removal - first remove contents, then directory
        // Note: This is for test cleanup only, not production use
        DIR* dir = opendir( path );
        if( dir )
        {
            struct dirent* entry;
            while( ( entry = readdir( dir ) ) != nullptr )
            {
                if( strcmp( entry->d_name, "." ) == 0 || strcmp( entry->d_name, ".." ) == 0 )
                    continue;
                
                std::string fullPath = std::string( path ) + "/" + entry->d_name;
                struct stat st;
                if( stat( fullPath.c_str(), &st ) == 0 )
                {
                    if( S_ISDIR( st.st_mode ) )
                    {
                        removeDirRecursive( fullPath.c_str() );
                    }
                    else
                    {
                        unlink( fullPath.c_str() );
                    }
                }
            }
            closedir( dir );
        }
        rmdir( path );
    }
    
    std::string m_path;
};

// Binary pattern generator for test data
class BinaryPattern
{
public:
    static std::vector<char> sequence( size_t size )
    {
        std::vector<char> result;
        result.reserve( size );
        for( size_t i = 0; i < size; i++ )
        {
            result.push_back( static_cast<char>( i & 0xFF ) );
        }
        return result;
    }
    
    static std::vector<char> random( size_t size )
    {
        std::vector<char> result;
        result.reserve( size );
        for( size_t i = 0; i < size; i++ )
        {
            result.push_back( static_cast<char>( rand() & 0xFF ) );
        }
        return result;
    }
    
    static std::vector<char> repeated( char value, size_t count )
    {
        return std::vector<char>( count, value );
    }
};

// Capture stdout/stderr for testing output
class OutputCapture
{
public:
    OutputCapture()
    {
        m_original = dup( STDOUT_FILENO );
        
        int pipefd[2];
        pipe( pipefd );
        
        dup2( pipefd[1], STDOUT_FILENO );
        close( pipefd[1] );
        
        m_readFd = pipefd[0];
    }
    
    ~OutputCapture()
    {
        dup2( m_original, STDOUT_FILENO );
        close( m_original );
        close( m_readFd );
    }
    
    std::string getOutput()
    {
        fflush( stdout );
        
        char buffer[4096];
        std::string result;
        
        int flags = fcntl( m_readFd, F_GETFL, 0 );
        fcntl( m_readFd, F_SETFL, flags | O_NONBLOCK );
        
        ssize_t n;
        while( ( n = read( m_readFd, buffer, sizeof( buffer ) ) ) > 0 )
        {
            result.append( buffer, n );
        }
        
        return result;
    }
    
private:
    int m_original;
    int m_readFd;
};
