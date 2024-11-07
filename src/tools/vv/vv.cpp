#include <getopt.h>
#include <memory>

#include "image/ImageLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/Callstack.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

    struct option longOptions[] = {
        { "debug", no_argument, nullptr, 'd' },
        { "external", no_argument, nullptr, 'e' },
    };

    int opt;
    while( ( opt = getopt_long( argc, argv, "de", longOptions, nullptr ) ) != -1 )
    {
        switch (opt)
        {
        case 'd':
            SetLogLevel( LogLevel::Callstack );
            break;
        case 'e':
            ShowExternalCallstacks( true );
            break;
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
    auto bitmap = std::unique_ptr<Bitmap>( LoadImage( imageFile ) );
    CheckPanic( bitmap, "Failed to load image" );

    return 0;
}
