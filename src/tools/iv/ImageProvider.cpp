#include <algorithm>
#include <ranges>
#include <tracy/Tracy.hpp>

#include "ImageProvider.hpp"
#include "image/ImageLoader.hpp"
#include "image/PngLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/DataBuffer.hpp"
#include "util/Logs.hpp"
#include "util/MemoryBuffer.hpp"
#include "util/TaskDispatch.hpp"

ImageProvider::ImageProvider( TaskDispatch& td )
    : m_shutdown( false )
    , m_currentJob( -1 )
    , m_nextId( 0 )
    , m_thread( [this] { Worker(); } )
    , m_td( td )
{
}

ImageProvider::~ImageProvider()
{
    {
        std::lock_guard lock( m_lock );
        m_shutdown.store( true, std::memory_order_release );
        m_cv.notify_all();
    }
    m_thread.join();
}

int64_t ImageProvider::LoadImage( const char* path, bool hdr, Callback callback, void* userData, Flags flags )
{
    ZoneScoped;
    const auto id = m_nextId++;
    std::lock_guard lock( m_lock );
    m_jobs.emplace_back( Job {
        .id = id,
        .path = path,
        .fd = -1,
        .hdr = hdr,
        .callback = callback,
        .userData = userData,
        .flags = flags
    } );
    m_cv.notify_one();
    return id;
}

int64_t ImageProvider::LoadImage( int fd, bool hdr, Callback callback, void* userData, const char* origin, Flags flags )
{
    ZoneScoped;
    const auto id = m_nextId++;
    std::lock_guard lock( m_lock );
    m_jobs.emplace_back( Job {
        .id = id,
        .path = origin,
        .fd = fd,
        .hdr = hdr,
        .callback = callback,
        .userData = userData,
        .flags = flags
    } );
    m_cv.notify_one();
    return id;
}

void ImageProvider::Cancel( int64_t id )
{
    ZoneScoped;
    std::unique_lock lock( m_lock );
    if( m_currentJob == id )
    {
        m_currentJob = -1;
    }
    else
    {
        auto it = std::ranges::find_if( m_jobs, [id]( const auto& job ) { return job.id == id; } );
        if( it != m_jobs.end() )
        {
            auto job = std::move( *it );
            m_jobs.erase( it );
            lock.unlock();
            job.callback( job.userData, job.id, Result::Cancelled, { .flags = job.flags } );
        }
    }
}

void ImageProvider::CancelAll()
{
    ZoneScoped;
    std::vector<Job> tmp;

    m_lock.lock();
    std::swap( tmp, m_jobs );
    m_currentJob = -1;
    m_lock.unlock();

    for( auto& job : tmp )
    {
        job.callback( job.userData, job.id, Result::Cancelled, { .flags = job.flags } );
    }
}

void ImageProvider::Worker()
{
    ZoneScoped;
    std::unique_lock lock( m_lock );
    while( !m_shutdown.load( std::memory_order_acquire ) )
    {
        m_currentJob = -1;
        m_cv.wait( lock, [this] { return !m_jobs.empty() || m_shutdown.load( std::memory_order_acquire ); } );
        if( m_shutdown.load( std::memory_order_acquire ) ) return;

        auto job = std::move( m_jobs.back() );
        m_jobs.pop_back();
        m_currentJob = job.id;
        lock.unlock();

        ZoneScopedN( "Image load" );
        std::unique_ptr<Bitmap> bitmap;
        std::unique_ptr<BitmapHdr> bitmapHdr;
        struct timespec mtime = {};

        if( job.fd >= 0 )
        {
            mclog( LogLevel::Info, "Loading image from file descriptor" );
            auto buffer = std::make_shared<MemoryBuffer>( job.fd );
            PngLoader loader( std::move( buffer ) );
            if( loader.IsValid() ) bitmap = loader.Load();
        }
        else
        {
            mclog( LogLevel::Info, "Loading image %s", job.path.c_str() );
            auto loader = GetImageLoader( job.path.c_str(), ToneMap::Operator::PbrNeutral, &m_td, &mtime );
            if( loader )
            {
                if( loader->IsHdr() && ( job.hdr || loader->PreferHdr() ) )
                {
                    bitmapHdr = loader->LoadHdr( job.hdr ? Colorspace::BT2020 : Colorspace::BT709 );
                    if( !job.hdr )
                    {
                        bitmap = std::make_unique<Bitmap>( bitmapHdr->Width(), bitmapHdr->Height() );
                        auto src = bitmapHdr->Data();
                        auto dst = bitmap->Data();
                        size_t sz = bitmapHdr->Width() * bitmapHdr->Height();
                        while( sz > 0 )
                        {
                            const auto chunk = std::min( sz, size_t( 16 * 1024 ) );
                            m_td.Queue( [src, dst, chunk] {
                                ToneMap::Process( ToneMap::Operator::PbrNeutral, (uint32_t*)dst, src, chunk );
                            } );
                            src += chunk * 4;
                            dst += chunk * 4;
                            sz -= chunk;
                        }
                        m_td.Sync();
                    }
                }
                else
                {
                    bitmap = loader->Load();
                }
            }
        }

        lock.lock();
        const bool cancelled = m_currentJob == -1;
        lock.unlock();

        if( cancelled )
        {
            job.callback( job.userData, job.id, Result::Cancelled, { .flags = job.flags } );
        }
        else if( bitmap || bitmapHdr )
        {
            uint32_t width, height;
            if( bitmap ) bitmap->NormalizeOrientation();
            mclog( LogLevel::Info, "Image loaded: %ux%u", bitmap ? bitmap->Width() : bitmapHdr->Width(), bitmap ? bitmap->Height() : bitmapHdr->Height() );
            job.callback( job.userData, job.id, Result::Success, {
                .bitmap = std::move( bitmap ),
                .bitmapHdr = std::move( bitmapHdr ),
                .origin = job.path,
                .flags = job.flags,
                .mtime = mtime
            } );
        }
        else
        {
            mclog( LogLevel::Error, "Failed to load image %s", job.path.c_str() );
            job.callback( job.userData, job.id, Result::Error, { .flags = job.flags } );
        }

        lock.lock();
    }
}
