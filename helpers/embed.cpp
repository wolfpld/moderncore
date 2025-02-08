#include <format>
#include <stdint.h>
#include <stdio.h>

void Usage()
{
    fprintf( stderr, "Usage: embed <objectName> <source> <destination>\n" );
    fprintf( stderr, "  destination should be without extension, will create cpp, hpp pair\n" );
}

int main( int argc, char** argv )
{
    if( argc < 4 )
    {
        Usage();
        return 1;
    }

    const char* objectName = argv[1];
    const char* source = argv[2];
    const char* destination = argv[3];

    FILE* src = fopen( source, "rb" );
    if( !src )
    {
        fprintf( stderr, "Failed to open source file %s\n", source );
        return 1;
    }

    size_t sz;
    fseek( src, 0, SEEK_END );
    sz = ftell( src );
    fseek( src, 0, SEEK_SET );

    auto data = new uint8_t[sz];
    fread( data, 1, sz, src );
    fclose( src );

    FILE* hdr = fopen( std::format( "{}.hpp", destination ).c_str(), "wb" );
    fprintf( hdr, "// This file is generated by embed tool, do not modify\n" );
    fprintf( hdr, "// Source: %s\n\n", source );
    fprintf( hdr, "#pragma once\n\n" );
    fprintf( hdr, "#include <stddef.h>\n" );
    fprintf( hdr, "#include <stdint.h>\n\n" );
    fprintf( hdr, "namespace Embed\n{\n" );
    fprintf( hdr, "constexpr size_t %sSize = %zu;\n", objectName, sz );
    fprintf( hdr, "extern const uint8_t %sData[];\n", objectName );
    fprintf( hdr, "}\n" );
    fclose( hdr );

    FILE* cpp = fopen( std::format( "{}.cpp", destination ).c_str(), "wb" );
    fprintf( cpp, "// This file is generated by embed tool, do not modify\n" );
    fprintf( cpp, "// Source: %s\n\n", source );
    fprintf( cpp, "#include \"%s.hpp\"\n\n", destination );
    fprintf( cpp, "namespace Embed\n{\n" );
    fprintf( cpp, "const uint8_t %sData[] =\n", objectName );
    fprintf( cpp, "{\n" );
    for( size_t i=0; i<sz; i++ )
    {
        fprintf( cpp, "%d,", data[i] );
    }
    fprintf( cpp, "};\n" );
    fprintf( cpp, "}\n" );

    fclose( cpp );
    delete[] data;
    return 0;
}
