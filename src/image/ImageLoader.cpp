#include <concepts>

#include "ImageLoader.hpp"
#include "JpgLoader.hpp"
#include "PngLoader.hpp"
#include "../util/Bitmap.hpp"
#include "../util/FileWrapper.hpp"
#include "../util/Home.hpp"
#include "../util/Logs.hpp"

template<typename T>
concept ImageLoader = requires( T loader, FileWrapper& file )
{
    { loader.IsValid() } -> std::convertible_to<bool>;
    { loader.Load() } -> std::convertible_to<Bitmap*>;
};

template<ImageLoader T>
static inline Bitmap* LoadImage( FileWrapper& file )
{
    T loader( file );
    if( !loader.IsValid() ) return nullptr;
    return loader.Load();
}

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

    Bitmap* img = LoadImage<PngLoader>( file );
    if( img ) return img;
    img = LoadImage<JpgLoader>( file );
    if( img ) return img;

    mclog( LogLevel::Error, "Failed to load image %s", path.c_str() );
    return nullptr;
}
