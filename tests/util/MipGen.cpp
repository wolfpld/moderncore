#include <catch2/catch_all.hpp>
#include <src/util/BitmapHdr.hpp>
#include <src/util/Colorspace.hpp>
#include <src/vulkan/shader/MipGenWeights.h>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
struct Tap
{
    int idx;
    float w;
};

void AxisTaps( int o, int srcSize, int dstSize, std::vector<Tap>& taps )
{
    taps.clear();

    const int n = Num( srcSize, dstSize );
    const int d = Den( srcSize, dstSize );
    const float ic = ( float( o ) + 0.5f ) * float( d ) / float( n ) - 0.5f;
    const int base = int( std::floor( ic ) );

    float total = 0.0f;
    for( int t = -8; t <= 8; t++ )
    {
        const float w = TapWeight( n, d, o, base + t, srcSize > dstSize ? 1 : 0 );
        if( w != 0.0f )
        {
            taps.push_back( { base + t, w } );
            total += w;
        }
    }

    const float tooSmallSum = 1.52587890625e-05f * float( n ) / float( d );
    if( taps.empty() || ( total < tooSmallSum && total > -tooSmallSum ) )
    {
        taps = { { 0, 0.0f } };
        return;
    }

    if( total < ( 1.0f - float( MIPGEN_TOO_SMALL ) ) || total > ( 1.0f + float( MIPGEN_TOO_SMALL ) ) )
    {
        const float filterScale = 1.0f / total;
        for( Tap& tap : taps ) tap.w = tap.w * filterScale;
    }
}

void DownscaleRef( const BitmapHdr& src, uint32_t dw, uint32_t dh, std::vector<float>& dst )
{
    const uint32_t sw = src.Width();
    const uint32_t sh = src.Height();
    dst.resize( size_t( dw ) * dh * 4 );

    std::vector<Tap> xTaps;
    std::vector<Tap> yTaps;

    for( uint32_t oy = 0; oy < dh; oy++ )
    {
        AxisTaps( int( oy ), int( sh ), int( dh ), yTaps );

        for( uint32_t ox = 0; ox < dw; ox++ )
        {
            AxisTaps( int( ox ), int( sw ), int( dw ), xTaps );

            float acc[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for( const Tap& yt : yTaps )
            {
                const int sy = std::min( std::max( yt.idx, 0 ), int( sh ) - 1 );
                float row[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                for( const Tap& xt : xTaps )
                {
                    const int sx = std::min( std::max( xt.idx, 0 ), int( sw ) - 1 );
                    const float* p = src.Data() + ( size_t( sy ) * sw + sx ) * 4;
                    const float a = p[3];
                    for( int c = 0; c < 3; c++ ) row[c] += xt.w * ( p[c] * a );
                    row[3] += xt.w * a;
                }
                for( int c = 0; c < 4; c++ ) acc[c] += yt.w * row[c];
            }

            float* o = dst.data() + ( size_t( oy ) * dw + ox ) * 4;
            o[3] = acc[3];
            if( acc[3] >= float( MIPGEN_ALPHA_TOO_SMALL ) )
            {
                const float inv = 1.0f / acc[3];
                for( int c = 0; c < 3; c++ ) o[c] = acc[c] * inv;
            }
            else
            {
                for( int c = 0; c < 3; c++ ) o[c] = acc[c];
            }
        }
    }
}

[[nodiscard]] float Tolerance( float alpha )
{
    return 2e-6f / std::max( alpha, 0.01f );
}

struct Fixture
{
    uint32_t sw, sh;
    uint32_t dw, dh;
    std::function<void( BitmapHdr& )> fill;
};

void RunParity( const Fixture& fx )
{
    BitmapHdr bmp( fx.sw, fx.sh, Colorspace::BT709 );
    fx.fill( bmp );

    auto resized = bmp.ResizeNew( fx.dw, fx.dh );
    REQUIRE( resized->Width() == fx.dw );
    REQUIRE( resized->Height() == fx.dh );

    std::vector<float> ref;
    DownscaleRef( bmp, fx.dw, fx.dh, ref );

    for( uint32_t y = 0; y < fx.dh; y++ )
    {
        for( uint32_t x = 0; x < fx.dw; x++ )
        {
            const float* a = ref.data() + ( size_t( y ) * fx.dw + x ) * 4;
            const float* b = resized->Data() + ( size_t( y ) * fx.dw + x ) * 4;
            const float tolA = 2e-6f;
            INFO( "pixel " << x << "," << y << " src " << fx.sw << "x" << fx.sh
                << " -> " << fx.dw << "x" << fx.dh );

            REQUIRE( std::abs( a[3] - b[3] ) <= tolA );
            const float tolRGB = std::max( Tolerance( a[3] ), Tolerance( b[3] ) );
            for( int c = 0; c < 3; c++ ) REQUIRE( std::abs( a[c] - b[c] ) <= tolRGB );
        }
    }
}

void FillGradient( BitmapHdr& bmp )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            float* d = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
            d[0] = float( x ) / float( std::max( 1u, bmp.Width() - 1 ) );
            d[1] = float( y ) / float( std::max( 1u, bmp.Height() - 1 ) );
            d[2] = 0.5f;
            d[3] = 1.0f;
        }
    }
}

void FillDeltas( BitmapHdr& bmp )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            float* d = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
            const bool spike = ( x % 7 == 3 ) && ( y % 5 == 2 );
            d[0] = spike ? 1.0f : 0.125f;
            d[1] = spike ? 0.25f : 0.5f;
            d[2] = 0.75f;
            d[3] = 1.0f;
        }
    }
}

void FillAlphaRamp( BitmapHdr& bmp )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            float* d = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
            const float t = float( x ) / float( std::max( 1u, bmp.Width() - 1 ) );
            const float a = t <= 0.5f ? 1.0f : std::max( 0.0f, 1.0f - 2.0f * ( t - 0.5f ) );
            d[0] = 0.9f;
            d[1] = a > 0.0f ? 0.3f : 1.0f;
            d[2] = a > 0.0f ? 0.3f : 0.0f;
            d[3] = a;
        }
    }
}
}

TEST_CASE( "MipGen tap math parity with stbir", "[mipgen][parity]" )
{
    RunParity( { 16, 10, 8, 5, FillGradient } );
    RunParity( { 16, 10, 8, 5, FillDeltas } );
    RunParity( { 5, 5, 2, 2, FillGradient } );
    RunParity( { 3, 3, 1, 1, FillDeltas } );
    RunParity( { 9, 9, 4, 4, FillDeltas } );
    RunParity( { 7, 7, 3, 3, FillGradient } );
    RunParity( { 16383, 4, 8192, 2, FillGradient } );
    RunParity( { 16383, 4, 8192, 2, FillDeltas } );
    RunParity( { 4, 1, 2, 1, FillGradient } );
    RunParity( { 8, 2, 4, 1, FillDeltas } );
    RunParity( { 17, 6, 8, 3, FillAlphaRamp } );
}

TEST_CASE( "MipGen tap math parity with stbir, non-power-of-two", "[mipgen][parity][npot]" )
{
    RunParity( { 9999, 7, 4999, 3, FillGradient } );
    RunParity( { 4095, 1023, 2047, 511, FillDeltas } );
    RunParity( { 65, 33, 32, 16, FillGradient } );
    RunParity( { 33, 65, 16, 32, FillDeltas } );
    RunParity( { 777, 123, 388, 61, FillGradient } );
    RunParity( { 1025, 31, 512, 15, FillDeltas } );

    RunParity( { 2047, 3, 1023, 1, FillGradient } );
    RunParity( { 3, 2047, 1, 1023, FillDeltas } );
    RunParity( { 511, 1, 255, 1, FillGradient } );
    RunParity( { 1, 511, 1, 255, FillDeltas } );

    RunParity( { 16383, 3, 8191, 1, FillGradient } );
    RunParity( { 16381, 5, 8190, 2, FillDeltas } );
}
