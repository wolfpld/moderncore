#include "ImageProvider.hpp"
#include "image/ImageLoader.hpp"
#include "util/Bitmap.hpp"
#include "util/BitmapHdr.hpp"
#include "util/Logs.hpp"

ImageProvider::ImageProvider()
    : m_td( std::max( 1u, std::thread::hardware_concurrency() - 1 ), "Image Provider" )
    , m_shutdown( false )
    , m_reaper( [this] { Reaper(); } )
{
}

ImageProvider::~ImageProvider()
{
    m_lock.lock();
    if( m_job ) m_job->cancel = true;
    m_shutdown.store( true, std::memory_order_release );
    m_reapCv.notify_all();
    m_lock.unlock();
    m_reaper.join();
    if( m_thread.joinable() ) m_thread.join();
    for( auto& t : m_reapPile ) t.join();
}

void ImageProvider::LoadImage( const char* path, Callback callback, void* userData )
{
    {
        std::lock_guard lock( m_lock );

        if( m_job )
        {
            m_job->cancel = true;
            m_job.reset();
            m_reapPile.emplace_back( std::move( m_thread ) );
            m_reapCv.notify_one();
        }
        else if( m_thread.joinable() )
        {
            m_thread.join();
        }
    }

    m_job = std::make_shared<Job>( Job {
        .path = path,
        .callback = callback,
        .userData = userData,
        .cancel = false
    } );

    m_thread = std::thread( [this] { Worker( m_job ); } );
}

void ImageProvider::CancelRequest()
{
    std::lock_guard lock( m_lock );
    if( m_job )
    {
        m_job->cancel = true;
        m_job.reset();
        m_reapPile.emplace_back( std::move( m_thread ) );
        m_reapCv.notify_one();
    }
}

// Worker signals that the job is done by reseting m_job
void ImageProvider::Worker( std::shared_ptr<Job> job )
{
    mclog( LogLevel::Info, "Loading image %s", job->path.c_str() );
    auto loader = GetImageLoader( job->path.c_str(), ToneMap::Operator::PbrNeutral, &m_td );
    if( !loader )
    {
        m_lock.lock();
        if( !job->cancel ) m_job.reset();
        m_lock.unlock();
        job->callback( job->userData, job->cancel ? Cancelled : Error, {} );
        return;
    }

    std::unique_ptr<Bitmap> bitmap;
    if( loader->IsHdr() && loader->PreferHdr() )
    {
        auto hdr = loader->LoadHdr();
        bitmap = std::make_unique<Bitmap>( hdr->Width(), hdr->Height() );

        m_lock.lock();
        const auto cancel = job->cancel;
        m_lock.unlock();
        if( cancel )
        {
            job->callback( job->userData, Cancelled, {} );
            return;
        }

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

    // TODO do this with geometry data
    bitmap->NormalizeOrientation();

    m_lock.lock();
    if( job->cancel )
    {
        m_lock.unlock();
        job->callback( job->userData, Cancelled, {} );
    }
    else
    {
        m_job.reset();
        m_lock.unlock();
        job->callback( job->userData, Success, std::move( bitmap ) );
    }
}

void ImageProvider::Reaper()
{
    std::unique_lock lock( m_lock );
    while( !m_shutdown.load( std::memory_order_acquire ) )
    {
        m_reapCv.wait( lock, [this] { return !m_reapPile.empty() || m_shutdown.load( std::memory_order_acquire ); } );
        if( m_reapPile.empty() ) continue;
        auto pile = std::move( m_reapPile );
        lock.unlock();
        for( auto& t : pile ) t.join();
        lock.lock();
    }
}
