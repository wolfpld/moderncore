#include "ImageLoader.hpp"
#include "PngLoader.hpp"
#include "../util/Bitmap.hpp"
#include "../util/FileWrapper.hpp"
#include "../util/Home.hpp"
#include "../util/Logs.hpp"

Bitmap* LoadImage( const char* filename )
{
    auto path = ExpandHome( filename );

    FileWrapper file( path.c_str(), "rb" );
    if( !file )
    {
        mclog( LogLevel::Error, "Background image %s does not exist.", path.c_str() );
        return nullptr;
    }

    mclog( LogLevel::Info, "Loading image %s", path.c_str() );

    {
        PngLoader loader( file );
        if( loader.IsValid() )
        {
            auto img = loader.Load();
            if( img ) return img;
        }
    }

    mclog( LogLevel::Error, "Failed to load image %s", path.c_str() );
    return nullptr;
}
