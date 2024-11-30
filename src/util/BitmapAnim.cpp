#include "BitmapAnim.hpp"

BitmapAnim::BitmapAnim( uint32_t frameCount )
{
    m_frames.reserve( frameCount );
}

void BitmapAnim::AddFrame( std::shared_ptr<Bitmap> bmp, uint32_t delay_us )
{
    m_frames.push_back( { std::move( bmp ), delay_us } );
}

void BitmapAnim::Resize( uint32_t width, uint32_t height )
{
    for( auto& frame : m_frames )
    {
        frame.bmp->Resize( width, height );
    }
}
