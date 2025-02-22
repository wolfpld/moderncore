#include <algorithm>

#include "ImageProvider.hpp"
#include "image/ImageLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Logs.hpp"

ImageProvider::ImageProvider()
    : m_td( std::max( 1u, std::thread::hardware_concurrency() - 1 ), "Image Provider" )
    , m_shutdown( false )
    , m_currentJob( -1 )
    , m_nextId( 0 )
    , m_thread( [this] { Worker(); } )
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

int64_t ImageProvider::LoadImage( const char* path, Callback callback, void* userData )
{
    const auto id = m_nextId++;
    std::lock_guard lock( m_lock );
    m_jobs.emplace_back( Job {
        .id = id,
        .path = path,
        .callback = callback,
        .userData = userData
    } );
    m_cv.notify_one();
    return id;
}

void ImageProvider::Cancel( int64_t id )
{
    std::lock_guard lock( m_lock );
    if( m_currentJob == id )
    {
        m_currentJob = -1;
    }
    else
    {
        auto it = std::find_if( m_jobs.begin(), m_jobs.end(), [id]( const auto& job ) { return job.id == id; } );
        if( it != m_jobs.end() )
        {
            m_jobs.erase( it );
        }
    }
}

void ImageProvider::CancelAll()
{
    std::vector<Job> tmp;

    m_lock.lock();
    std::swap( tmp, m_jobs );
    m_currentJob = -1;
    m_lock.unlock();

    for( auto& job : tmp )
    {
        job.callback( job.userData, job.id, Result::Cancelled, nullptr );
    }
}

void ImageProvider::Worker()
{
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

        std::unique_ptr<Bitmap> bitmap;

        mclog( LogLevel::Info, "Loading image %s", job.path.c_str() );
        auto loader = GetImageLoader( job.path.c_str(), ToneMap::Operator::PbrNeutral, &m_td );
        if( loader )
        {
            if( loader->IsHdr() && loader->PreferHdr() )
            {
                auto hdr = loader->LoadHdr();
                bitmap = std::make_unique<Bitmap>( hdr->Width(), hdr->Height() );

                auto src = hdr->Data();
                auto dst = bitmap->Data();
                size_t sz = hdr->Width() * hdr->Height();
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
            else
            {
                bitmap = loader->Load();
            }
        }

        lock.lock();
        if( m_currentJob == -1 )
        {
            job.callback( job.userData, job.id, Result::Cancelled, nullptr );
        }
        else if( bitmap )
        {
            mclog( LogLevel::Info, "Image loaded: %ux%u", bitmap->Width(), bitmap->Height() );
            job.callback( job.userData, job.id, Result::Success, std::move( bitmap ) );
        }
        else
        {
            mclog( LogLevel::Error, "Failed to load image %s", job.path.c_str() );
            job.callback( job.userData, job.id, Result::Error, nullptr );
        }
    }
}
