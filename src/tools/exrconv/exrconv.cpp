#include <algorithm>
#include <stdio.h>
#include <thread>

#include <ImfRgbaFile.h>

#include "image/ImageLoader.hpp"
#include "util/Ansi.hpp"
#include "util/BitmapHdr.hpp"
#include "util/BitmapHdrHalf.hpp"
#include "util/Home.hpp"
#include "util/Logs.hpp"
#include "util/Panic.hpp"
#include "util/TaskDispatch.hpp"
#include "util/Tonemapper.hpp"
#include "GitRef.hpp"

static void PrintHelp()
{
    printf( ANSI_BOLD ANSI_GREEN "exrconv" ANSI_RESET " — convert HDR image to EXR format, build %s\n\n", GitRef );
    printf( "Usage: exrconv <input> <output>\n" );
}

int main( int argc, char** argv )
{
#ifdef NDEBUG
    SetLogLevel( LogLevel::Error );
#endif

    if( argc != 3 )
    {
        PrintHelp();
        return 1;
    }

    const auto workerThreads = std::max( 1u, std::thread::hardware_concurrency() - 1 );
    TaskDispatch td( workerThreads, "Worker" );

    const auto inFileStr = ExpandHome( argv[1] );
    const auto inFile = inFileStr.c_str();

    const auto outFileStr = ExpandHome( argv[2] );
    const auto outFile = outFileStr.c_str();

    mclog( LogLevel::Info, "Converting %s to %s", inFile, outFile );

    auto loader = GetImageLoader( inFile, ToneMap::Operator::PbrNeutral, &td );
    if( !loader )
    {
        mclog( LogLevel::Error, "Failed to load image %s", inFile );
        return 1;
    }
    if( !loader->IsHdr() )
    {
        mclog( LogLevel::Error, "Image %s is not HDR", inFile );
        return 1;
    }

    auto hdr = loader->LoadHdr();
    CheckPanic( hdr, "Failed to load image %s", inFile );

    auto half = std::make_unique<BitmapHdrHalf>( *hdr );

    Imf::RgbaOutputFile output( outFile, hdr->Width(), hdr->Height(), Imf::WRITE_RGBA );
    output.setFrameBuffer( (const Imf::Rgba*)half->Data(), 1, hdr->Width() );
    output.writePixels( hdr->Height() );

    return 0;
}
