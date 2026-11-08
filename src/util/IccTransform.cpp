#include "util/IccTransform.hpp"

#include "util/TaskDispatch.hpp"

#include <algorithm>

IccTransform::IccTransform( cmsHPROFILE profileIn, cmsHPROFILE profileOut )
: m_fast( profileIn, profileOut )
{
    if( !profileIn || !profileOut ) return;

    if( !m_fast )
    {
        m_transforms[u8ToFloat] = cmsCreateTransform( profileIn, TYPE_RGBA_8, profileOut, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
        if( !m_transforms[u8ToFloat] )
        {
            m_srgb = cmsCreate_sRGBProfile();
            if( m_srgb )
            {
                m_transforms[recoverToSrgb] = cmsCreateTransform( profileIn, TYPE_RGBA_8, m_srgb, TYPE_RGBA_8, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
                m_transforms[srgbToFloat] = cmsCreateTransform( m_srgb, TYPE_RGBA_8, profileOut, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
                if( !m_transforms[recoverToSrgb] || !m_transforms[srgbToFloat] )
                {
                    cmsCloseProfile( m_srgb );
                    m_srgb = nullptr;
                }
            }
        }
    }

    if( !m_fast.SupportsFloat() )
    {
        m_transforms[floatToFloat] = cmsCreateTransform( profileIn, TYPE_RGBA_FLT, profileOut, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
        m_transforms[halfToHalf] = cmsCreateTransform( profileIn, TYPE_RGBA_HALF_FLT, profileOut, TYPE_RGBA_HALF_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    }
}

IccTransform::~IccTransform()
{
    for( auto t : m_transforms )
    {
        if( t ) cmsDeleteTransform( t );
    }
    if( m_srgb ) cmsCloseProfile( m_srgb );
}

bool IccTransform::RunLcms( Kind kind, TaskDispatch* td, const void* input, void* output, size_t pixels, size_t inStride, size_t outStride ) const
{
    auto transform = m_transforms[kind];
    if( !transform ) return false;

    if( !td )
    {
        auto in = (const uint8_t*)input;
        auto out = (uint8_t*)output;
        auto size = pixels;
        while( size > 0 )
        {
            const auto chunk = std::min<size_t>( size, UINT32_MAX );
            cmsDoTransform( transform, in, out, (cmsUInt32Number)chunk );
            in += chunk * inStride;
            out += chunk * outStride;
            size -= chunk;
        }
    }
    else
    {
        auto in = (const uint8_t*)input;
        auto out = (uint8_t*)output;
        auto size = pixels;
        while( size > 0 )
        {
            auto chunk = std::min<size_t>( size, 256 * 1024 );
            td->Queue( [transform, in, out, chunk] {
                cmsDoTransform( transform, in, out, (cmsUInt32Number)chunk );
            } );
            in += chunk * inStride;
            out += chunk * outStride;
            size -= chunk;
        }
        td->Sync();
    }
    return true;
}

bool IccTransform::Run( TaskDispatch* td, uint8_t* input, float* output, size_t pixels ) const
{
    if( m_fast.Run( td, input, output, pixels ) ) return true;

    if( m_transforms[u8ToFloat] )
    {
        return RunLcms( u8ToFloat, td, input, output, pixels, 4, 16 );
    }
    if( !RunLcms( recoverToSrgb, td, input, input, pixels, 4, 4 ) )
    {
        return false;
    }
    return RunLcms( srgbToFloat, td, input, output, pixels, 4, 16 );
}

bool IccTransform::RunFloat( TaskDispatch* td, const float* input, float* output, size_t pixels ) const
{
    if( m_fast.RunFloat( td, input, output, pixels ) ) return true;
    return RunLcms( floatToFloat, td, input, output, pixels, 16, 16 );
}

bool IccTransform::RunHalf( TaskDispatch* td, uint16_t* data, size_t pixels ) const
{
    if( m_fast.RunHalf( td, data, pixels ) ) return true;
    return RunLcms( halfToHalf, td, data, data, pixels, 8, 8 );
}
