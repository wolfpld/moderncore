#include <concepts>
#include <stdint.h>
#include <sys/stat.h>
#include <tracy/Tracy.hpp>

#include "DdsLoader.hpp"
#include "ExrLoader.hpp"
#include "HeifLoader.hpp"
#include "ImageLoader.hpp"
#include "JpgLoader.hpp"
#include "JxlLoader.hpp"
#include "PcxLoader.hpp"
#include "PngLoader.hpp"
#include "PvrLoader.hpp"
#include "RawLoader.hpp"
#include "StbImageLoader.hpp"
#include "TiffLoader.hpp"
#include "WebpLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapAnim.hpp"
#include "util/BitmapHdr.hpp"
#include "util/FileWrapper.hpp"
#include "util/Logs.hpp"
#include "vector/PdfImage.hpp"
#include "vector/SvgImage.hpp"

template<typename T>
concept ImageLoaderConcept = requires( T loader, const std::shared_ptr<FileWrapper>& file )
{
    { loader.IsValid() } -> std::convertible_to<bool>;
    { loader.Load() } -> std::convertible_to<std::unique_ptr<Bitmap>>;
};

template<ImageLoaderConcept T, typename... Args>
static inline std::unique_ptr<ImageLoader> CheckImageLoader( const std::shared_ptr<FileWrapper>& file, Args&&... args )
{
    auto loader = std::make_unique<T>( file, std::forward<Args>( args )... );
    if( loader->IsValid() ) return loader;
    return nullptr;
}

template<ImageLoaderConcept T, typename... Args>
static inline std::unique_ptr<ImageLoader> CheckImageLoader( const uint8_t* buf, size_t size, const std::shared_ptr<FileWrapper>& file, Args&&... args )
{
    if( !T::IsValidSignature( buf, size ) ) return nullptr;
    auto loader = std::make_unique<T>( file, std::forward<Args>( args )... );
    if( loader->IsValid() ) return loader;
    return nullptr;
}

std::unique_ptr<BitmapAnim> ImageLoader::LoadAnim()
{
    return nullptr;
}

std::unique_ptr<BitmapHdr> ImageLoader::LoadHdr( Colorspace colorspace )
{
    return nullptr;
}

std::unique_ptr<ImageLoader> GetImageLoader( const char* path, ToneMap::Operator tonemap, TaskDispatch* td, struct timespec* mtime )
{
    ZoneScoped;

    auto file = std::make_shared<FileWrapper>( path, "rb" );
    if( !*file )
    {
        mclog( LogLevel::Error, "Image %s does not exist.", path );
        return nullptr;
    }

    uint8_t buf[12];       // Loaders test at most 12 bytes.
    const size_t sz = fread( buf, 1, sizeof( buf ), *file );
    if( sz == 0 )
    {
        mclog( LogLevel::Error, "Image %s is empty.", path );
        return nullptr;
    }

    if( mtime )
    {
        struct stat st;
        if( fstat( fileno( *file ), &st ) == 0 ) *mtime = st.st_mtim;
    }

    if( auto loader = CheckImageLoader<PngLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<JpgLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<JxlLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<WebpLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<HeifLoader>( buf, sz, file, tonemap, td ); loader ) return loader;
    if( auto loader = CheckImageLoader<PvrLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<DdsLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<PcxLoader>( buf, sz, file ); loader ) return loader;
    if( auto loader = CheckImageLoader<StbImageLoader>( file ); loader ) return loader;
    if( auto loader = CheckImageLoader<ExrLoader>( buf, sz, file, tonemap, td ); loader ) return loader;
    if( auto loader = CheckImageLoader<RawLoader>( file ); loader ) return loader;
    if( auto loader = CheckImageLoader<TiffLoader>( buf, sz, file ); loader ) return loader;

    mclog( LogLevel::Debug, "Raster image loaders can't open %s", path );
    return nullptr;
}

std::unique_ptr<Bitmap> LoadImage( const char* path )
{
    ZoneScoped;
    mclog( LogLevel::Info, "Loading image %s", path );

    auto loader = GetImageLoader( path, ToneMap::Operator::PbrNeutral );
    if( loader ) return loader->Load();
    return nullptr;
}

std::unique_ptr<VectorImage> LoadVectorImage( const char* path )
{
    ZoneScoped;

    FileWrapper file( path, "rb" );
    if( !file )
    {
        mclog( LogLevel::Error, "Vector image %s does not exist.", path );
        return nullptr;
    }

    mclog( LogLevel::Info, "Loading vector image %s", path );

    if( auto img = std::make_unique<SvgImage>( file ); img->IsValid() ) return img;
    if( auto img = std::make_unique<PdfImage>( file ); img->IsValid() ) return img;

    mclog( LogLevel::Info, "Vector loaders can't open %s", path );
    return nullptr;
}
