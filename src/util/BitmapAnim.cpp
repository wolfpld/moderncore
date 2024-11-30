#include "BitmapAnim.hpp"

BitmapAnim::BitmapAnim( uint32_t frameCount )
{
    m_frames.reserve( frameCount );
}

void BitmapAnim::AddFrame( std::shared_ptr<Bitmap> bmp, uint32_t delay_us )
{
    m_frames.push_back( { std::move( bmp ), delay_us } );
}
