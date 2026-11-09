#include <catch2/catch_all.hpp>
#include <contrib/half.hpp>
#include <lcms2.h>
#include <src/util/Colorspace.hpp>
#include <src/util/FastIccTransform.hpp>
#include <src/util/TaskDispatch.hpp>
#include <cmath>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

namespace
{

struct ProfilePair
{
    cmsHPROFILE in;
    cmsHPROFILE out;
    ProfilePair()
    {
        cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
        cmsToneCurve* linear3[3] = { linear, linear, linear };
        in = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );
        out = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
        cmsFreeToneCurve( linear );
    }
    ~ProfilePair()
    {
        cmsCloseProfile( in );
        cmsCloseProfile( out );
    }
};

size_t FillFloat( std::vector<float>& buf )
{
    size_t seed = 12345;
    for( size_t i = 0; i < buf.size(); i += 4 )
    {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = float( ( seed >> 40 ) % 4000 ) / 1000.f;
        buf[i + 1] = float( ( seed >> 24 ) % 3500 ) / 1000.f;
        buf[i + 2] = float( ( seed >> 8 ) % 3000 ) / 1000.f;
        buf[i + 3] = float( ( seed >> 56 ) % 101 ) / 100.f;
    }
    return buf.size() / 4;
}

}

TEST_CASE( "FastIccTransform linear float run matches lcms", "[fasticc][colorspace]" )
{
    ProfilePair pp;
    const FastIccTransform fs( pp.in, pp.out );
    REQUIRE( static_cast<bool>( fs ) );
    REQUIRE( fs.SupportsFloat() );

    std::vector<float> buf( 600001 * 4 );
    const size_t px = FillFloat( buf );
    std::vector<float> ref( buf.size() );

    auto transform = cmsCreateTransform( pp.in, TYPE_RGBA_FLT, pp.out, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( transform );
    cmsDoTransform( transform, buf.data(), ref.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( transform );

    std::vector<float> out( buf.size() );
    fs.RunFloat( nullptr, buf.data(), out.data(), px );

    for( size_t i = 0; i < out.size(); i += 4 )
    {
        for( int c = 0; c < 3; c++ )
        {
            CHECK( out[i + c] == Catch::Approx( ref[i + c] ).margin( 1e-5f ) );
        }
        CHECK( out[i + 3] == ref[i + 3] );
    }
}

TEST_CASE( "FastIccTransform byte run matches lcms", "[fasticc][colorspace]" )
{
    ProfilePair pp;
    const FastIccTransform fs( pp.in, pp.out );
    REQUIRE( static_cast<bool>( fs ) );

    const size_t px = 600001;
    std::vector<uint8_t> buf( px * 4 );
    size_t seed = 54321;
    for( size_t i = 0; i < px * 4; i += 4 )
    {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        buf[i] = (uint8_t)( seed >> 40 );
        buf[i + 1] = (uint8_t)( seed >> 28 );
        buf[i + 2] = (uint8_t)( seed >> 16 );
        buf[i + 3] = 255;
    }

    std::vector<float> ref( px * 4 );
    auto transform = cmsCreateTransform( pp.in, TYPE_RGBA_8, pp.out, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( transform );
    cmsDoTransform( transform, buf.data(), ref.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( transform );

    std::vector<float> out( px * 4 );
    fs.Run( nullptr, buf.data(), out.data(), px );

    for( size_t i = 0; i < out.size(); i += 4 )
    {
        for( int c = 0; c < 3; c++ )
        {
            float want = ref[i + c];
            want = want < 0.f ? 0.f : want;
            want = want > 1.f ? 1.f : want;
            CHECK( out[i + c] == Catch::Approx( want ).margin( 1e-5f ) );
        }
        CHECK( out[i + 3] == 1.f );
    }

    TaskDispatch td( 4, "test" );
    std::vector<float> threaded( px * 4 );
    fs.Run( &td, buf.data(), threaded.data(), px );
    for( size_t i = 0; i < out.size(); i++ )
    {
        CHECK( threaded[i] == out[i] );
    }

    std::vector<float> pad( px * 4 + 64 );
    float* unaligned = (float*)( ( ( (uintptr_t)pad.data() + 64 ) & ~(uintptr_t)63 ) + 32 );
    void* raw = nullptr;
    REQUIRE( posix_memalign( &raw, 64, px * 4 * sizeof( float ) ) == 0 );
    float* aligned = (float*)raw;
    fs.Run( nullptr, buf.data(), unaligned, px );
    fs.Run( nullptr, buf.data(), aligned, px );
    for( size_t i = 0; i < px * 4; i++ )
    {
        CHECK( aligned[i] == unaligned[i] );
    }
    free( raw );
}

TEST_CASE( "FastIccTransform float run is placement and thread count agnostic", "[fasticc][colorspace]" )
{
    ProfilePair pp;
    const FastIccTransform fs( pp.in, pp.out );
    std::vector<float> src( 600001 * 4 );
    const size_t px = FillFloat( src );

    std::vector<float> inplace( src );
    fs.RunFloat( nullptr, inplace.data(), inplace.data(), px );

    std::vector<float> sep( src );
    std::vector<float> dst( src.size() );
    fs.RunFloat( nullptr, sep.data(), dst.data(), px );
    for( size_t i = 0; i < inplace.size(); i++ )
    {
        CHECK( dst[i] == inplace[i] );
    }

    TaskDispatch td( 4, "test" );
    std::vector<float> threaded( src );
    fs.RunFloat( &td, threaded.data(), threaded.data(), px );
    for( size_t i = 0; i < inplace.size(); i++ )
    {
        CHECK( threaded[i] == inplace[i] );
    }
}

TEST_CASE( "FastIccTransform linear half run matches lcms", "[fasticc][colorspace]" )
{
    ProfilePair pp;
    const FastIccTransform fs( pp.in, pp.out );

    std::vector<float> fbuf( ( 4096 + 3 ) * 4 );
    const size_t px = FillFloat( fbuf );
    std::vector<half_float::half> data( fbuf.size() );
    for( size_t i = 0; i < fbuf.size(); i++ )
    {
        data[i] = half_float::half( fbuf[i] );
    }
    std::vector<half_float::half> ref( data );

    auto transform = cmsCreateTransform( pp.in, TYPE_RGBA_HALF_FLT, pp.out, TYPE_RGBA_HALF_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( transform );
    cmsDoTransform( transform, ref.data(), ref.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( transform );

    fs.RunHalf( nullptr, (uint16_t*)data.data(), px );

    for( size_t i = 0; i < data.size(); i += 4 )
    {
        for( int c = 0; c < 3; c++ )
        {
            const float got = float( data[i + c] );
            const float want = float( ref[i + c] );
            CHECK( got == Catch::Approx( want ).margin( std::fabs( want ) * 0.003f + 0.002f ) );
        }
        CHECK( data[i + 3] == ref[i + 3] );
    }
}

TEST_CASE( "FastIccTransform gates non-linear profiles out of float runs", "[fasticc][colorspace]" )
{
    auto srgb = cmsCreate_sRGBProfile();
    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };
    auto out = cmsCreateRGBProfile( &white709, &primaries709, linear3 );

    const FastIccTransform fs( srgb, out );
    CHECK( static_cast<bool>( fs ) );
    CHECK( false == fs.SupportsFloat() );

    cmsCloseProfile( srgb );
    cmsCloseProfile( out );
    cmsFreeToneCurve( linear );
}
