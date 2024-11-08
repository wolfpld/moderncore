#include <libbase64.h>
#include <format>
#include <getopt.h>
#include <memory>
#include <thread>
#include <stdio.h>
#include <sys/ioctl.h>

#include "Terminal.hpp"
#include "image/ImageLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"

namespace {
void PrintHelp()
{
    printf( "Usage: vv [options]\n" );
    printf( "Options:\n" );
    printf( "  -d, --debug                  Enable debug logging\n" );
    printf( "  -e, --external               Show external callstacks\n" );
    printf( "  -b, --block                  Use text-only block mode\n" );
    printf( "  --help                       Print this help\n" );
}

void AdjustBitmap( Bitmap& bitmap, uint32_t col, uint32_t row )
{
    if( bitmap.Width() > col || bitmap.Height() > row )
    {
        const auto ratio = std::min( float( col ) / bitmap.Width(), float( row ) / bitmap.Height() );
        bitmap.Resize( bitmap.Width() * ratio, bitmap.Height() * ratio );
        mclog( LogLevel::Info, "Image resized: %ux%u", bitmap.Width(), bitmap.Height() );
    }
}
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

    enum { OptHelp };

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
        { "block", no_argument, nullptr, 'b' },
        { "help", no_argument, nullptr, OptHelp },
    };

    bool blockMode = false;

    int opt;
    while( ( opt = getopt_long( argc, argv, "deb", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Callstack );
            break;
        case 'e':
            ShowExternalCallstacks( true );
            break;
        case 'b':
            blockMode = true;
            break;
        case OptHelp:
            PrintHelp();
            return 0;
        default:
            break;
        }
    }
    if (optind == argc)
    {
        mclog( LogLevel::Error, "Image file name must be provided" );
        return 1;
    }

    const char* imageFile = argv[optind];
    std::unique_ptr<Bitmap> bitmap;
    auto imageThread = std::thread( [&bitmap, imageFile] {
        bitmap.reset( LoadImage( imageFile ) );
        CheckPanic( bitmap, "Failed to load image" );
        mclog( LogLevel::Info, "Image loaded: %ux%u", bitmap->Width(), bitmap->Height() );
    } );

    struct winsize ws;
    ioctl( 0, TIOCGWINSZ, &ws );
    mclog( LogLevel::Info, "Terminal size: %dx%d", ws.ws_col, ws.ws_row );

    int cw, ch;
    if( !blockMode )
    {
        if( !OpenTerminal() )
        {
            mclog( LogLevel::Error, "Failed to open terminal" );
            blockMode = true;
        }
        else
        {
            const auto charSizeResp = QueryTerminal( "\033[16t" );
            if( sscanf( charSizeResp.c_str(), "\033[6;%d;%dt", &ch, &cw ) != 2 )
            {
                mclog( LogLevel::Error, "Failed to query terminal character size" );
                blockMode = true;
            }
            else
            {
                mclog( LogLevel::Info, "Terminal char size: %dx%d", cw, ch );
            }

            CloseTerminal();
        }
    }

    imageThread.join();

    if( blockMode )
    {
        uint32_t col = ws.ws_col;
        uint32_t row = std::max<uint16_t>( 1, ws.ws_row - 1 ) * 2;

        mclog( LogLevel::Info, "Virtual pixels: %ux%u", col, row );
        AdjustBitmap( *bitmap, col, row );

        auto px0 = (uint32_t*)bitmap->Data();
        auto px1 = px0 + bitmap->Width();

        for( int y=0; y<bitmap->Height() / 2; y++ )
        {
            for( int x=0; x<bitmap->Width(); x++ )
            {
                auto c0 = *px0++;
                auto c1 = *px1++;
                auto r0 = ( c0       ) & 0xFF;
                auto g0 = ( c0 >> 8  ) & 0xFF;
                auto b0 = ( c0 >> 16 ) & 0xFF;
                auto r1 = ( c1       ) & 0xFF;
                auto g1 = ( c1 >> 8  ) & 0xFF;
                auto b1 = ( c1 >> 16 ) & 0xFF;
                printf( "\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm▀", r0, g0, b0, r1, g1, b1 );
            }
            printf( "\033[0m\n" );
            px0 += bitmap->Width();
            px1 += bitmap->Width();
        }
        if( ( bitmap->Height() & 1 ) != 0 )
        {
            for( int x=0; x<bitmap->Width(); x++ )
            {
                auto c0 = *px0++;
                auto r0 = ( c0       ) & 0xFF;
                auto g0 = ( c0 >> 8  ) & 0xFF;
                auto b0 = ( c0 >> 16 ) & 0xFF;
                printf( "\033[38;2;%d;%d;%dm▀", r0, g0, b0 );
            }
            printf( "\033[0m\n" );
        }
    }
    else
    {
        uint32_t col = ws.ws_col * cw;
        uint32_t row = std::max<uint16_t>( 1, ws.ws_row - 1 ) * ch;

        mclog( LogLevel::Info, "Pixels available: %ux%u", col, row );
        AdjustBitmap( *bitmap, col, row );

        size_t bmpSize = bitmap->Width() * bitmap->Height() * 4;
        size_t b64Size = ( ( 4 * bmpSize / 3 ) + 3 ) & ~3;
        char* b64Data = new char[b64Size+1];
        b64Data[b64Size] = 0;
        size_t outSize;
        base64_encode( (const char*)bitmap->Data(), bmpSize, b64Data, &outSize, 0 );
        CheckPanic( outSize == b64Size, "Base64 encoding failed" );
        mclog( LogLevel::Info, "Base64 size: %zu", b64Size );

        if( b64Size <= 4096 )
        {
            std::string payload = std::format( "\033_Gf=32,s={},v={},a=T;{}\033\\", bitmap->Width(), bitmap->Height(), b64Data );
            write( STDOUT_FILENO, payload.c_str(), payload.size() );
        }
        else
        {
            auto ptr = b64Data;
            while( b64Size > 0 )
            {
                size_t chunkSize = std::min<size_t>( 4096, b64Size );
                b64Size -= chunkSize;

                std::string payload;
                if( ptr == b64Data )
                {
                    payload = std::format( "\033_Gf=32,s={},v={},a=T,m=1;", bitmap->Width(), bitmap->Height() );
                }
                else if( b64Size > 0 )
                {
                    payload = "\033_Gm=1;";
                }
                else
                {
                    payload = "\033_Gm=0;";
                }
                payload.append( ptr, chunkSize );
                payload.append( "\033\\" );
                write( STDOUT_FILENO, payload.c_str(), payload.size() );

                ptr += chunkSize;
            }
        }

        if( bitmap->Width() < col ) printf( "\n" );

        delete[] b64Data;
    }

    return 0;
}
