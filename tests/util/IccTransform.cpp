#include <catch2/catch_all.hpp>
#include <contrib/half.hpp>
#include <lcms2.h>
#include <src/util/Colorspace.hpp>
#include <src/util/IccTransform.hpp>
#include <cmath>
#include <vector>

namespace
{

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

double MaxDiff( const std::vector<float>& a, const std::vector<float>& b )
{
    double d = 0.;
    for( size_t i = 0; i < a.size(); i++ )
    {
        if( i % 4 == 3 ) continue;
        d = std::max( d, (double)std::abs( a[i] - b[i] ) );
    }
    return d;
}

}

TEST_CASE( "IccTransform uses the fast path when the pair qualifies", "[icctransform][colorspace]" )
{
    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };
    auto in = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
    auto out = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );

    std::vector<float> buf( ( 65536 + 26 ) * 4 );
    const size_t px = FillFloat( buf );
    std::vector<float> want( buf.size() );
    auto lcms = cmsCreateTransform( in, TYPE_RGBA_8, out, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( lcms );

    auto raw = std::vector<uint8_t>( px * 4 );
    for( size_t i = 0; i < raw.size(); i++ )
    {
        raw[i] = (uint8_t)( i * 7 + 3 );
    }
    cmsDoTransform( lcms, raw.data(), want.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( lcms );

    const IccTransform t( in, out );
    auto data = raw;
    auto got = std::vector<float>( want.size() );
    REQUIRE( t.Run( nullptr, data.data(), got.data(), px ) );
    for( size_t i = 0; i < got.size(); i += 4 )
    {
        for( int c = 0; c < 3; c++ )
        {
            CHECK( got[i + c] == Catch::Approx( want[i + c] ).margin( 1e-5f ) );
        }
        CHECK( got[i + 3] == 1.f );
    }

    cmsCloseProfile( in );
    cmsCloseProfile( out );
    cmsFreeToneCurve( linear );
}

TEST_CASE( "IccTransform falls back to lcms for unsupported pairs", "[icctransform][colorspace]" )
{
    cmsToneCurve* gamma = cmsBuildGamma( nullptr, 2.2 );
    cmsToneCurve* gamma3[3] = { gamma, gamma, gamma };
    auto in = cmsCreateRGBProfile( &white709, &primaries709, gamma3 );
    auto out = cmsCreateRGBProfile( &white709, &primaries2020, gamma3 );

    const IccTransform t( in, out );
    CHECK( false == static_cast<bool>( FastIccTransform( in, out ) ) );
    CHECK( false == FastIccTransform( in, out ).SupportsFloat() );

    std::vector<float> buf( 4096 * 4 );
    const size_t px = FillFloat( buf );
    std::vector<float> ref( buf.size() );

    auto transform = cmsCreateTransform( in, TYPE_RGBA_FLT, out, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( transform );
    cmsDoTransform( transform, buf.data(), ref.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( transform );

    std::vector<float> data( buf );
    REQUIRE( t.RunFloat( nullptr, data.data(), data.data(), px ) );
    for( size_t i = 0; i < data.size(); i++ )
    {
        CHECK( data[i] == Catch::Approx( ref[i] ).margin( 1e-5f ) );
    }

    auto halfRef = std::vector<half_float::half>( data.size() );
    for( size_t i = 0; i < data.size(); i++ )
    {
        halfRef[i] = half_float::half( ref[i] );
    }
    auto halfData = halfRef;
    for( size_t i = 0; i < buf.size(); i++ )
    {
        halfData[i] = half_float::half( buf[i] );
    }
    REQUIRE( t.RunHalf( nullptr, (uint16_t*)halfData.data(), px ) );
    for( size_t i = 0; i < halfData.size(); i += 4 )
    {
        for( int c = 0; c < 3; c++ )
        {
            const float got = float( halfData[i+c] );
            const float want = float( halfRef[i+c] );
            CHECK( got == Catch::Approx( want ).margin( std::fabs( want ) * 0.003f + 0.002f ) );
        }
    }

    std::vector<uint8_t> u8( px * 4 );
    for( size_t i = 0; i < u8.size(); i++ )
    {
        u8[i] = (uint8_t)( i * 7 + 3 );
    }
    std::vector<float> u8Out( px * 4 );
    REQUIRE( t.Run( nullptr, u8.data(), u8Out.data(), px ) );

    std::vector<float> want( px * 4 );
    auto u8Ref = cmsCreateTransform( in, TYPE_RGBA_8, out, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( u8Ref );
    cmsDoTransform( u8Ref, u8.data(), want.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( u8Ref );
    for( size_t i = 0; i < u8Out.size(); i++ )
    {
        CHECK( u8Out[i] == Catch::Approx( want[i] ).margin( 1e-4f ) );
    }

    cmsCloseProfile( in );
    cmsCloseProfile( out );
    cmsFreeToneCurve( gamma );
}

TEST_CASE( "IccTransform falls back to lcms when the input carries a CLUT", "[icctransform][colorspace]" )
{
    cmsToneCurve* linear = cmsBuildGamma( nullptr, 1 );
    cmsToneCurve* linear3[3] = { linear, linear, linear };
    auto in = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
    auto out = cmsCreateRGBProfile( &white709, &primaries2020, linear3 );
    REQUIRE( in );
    REQUIRE( out );

    cmsUInt16Number grid[8*3];
    for( int i = 0; i < 8; i++ )
    {
        grid[i*3+0] = ( i & 1 ) ? 55000 : 7000;
        grid[i*3+1] = ( i & 2 ) ? 9000 : 52000;
        grid[i*3+2] = ( i & 4 ) ? 51000 : 6000;
    }
    auto pipe = cmsPipelineAlloc( nullptr, 3, 3 );
    REQUIRE( pipe );
    cmsStage* curves = cmsStageAllocToneCurves( nullptr, 3, linear3 );
    cmsStage* clut = cmsStageAllocCLut16bit( nullptr, 2, 3, 3, grid );
    REQUIRE( curves );
    REQUIRE( clut );
    REQUIRE( cmsPipelineInsertStage( pipe, cmsAT_END, curves ) );
    REQUIRE( cmsPipelineInsertStage( pipe, cmsAT_END, clut ) );
    REQUIRE( cmsWriteTag( in, cmsSigAToB0Tag, pipe ) );
    cmsPipelineFree( pipe );

    CHECK( cmsIsMatrixShaper( in ) );
    CHECK( cmsIsCLUT( in, IccRenderIntent, LCMS_USED_AS_INPUT ) );
    CHECK( false == static_cast<bool>( FastIccTransform( in, out ) ) );

    std::vector<uint8_t> u8( 4096 * 4 );
    for( size_t i = 0; i < u8.size(); i++ )
    {
        u8[i] = (uint8_t)( i * 7 + 3 );
    }
    const size_t px = u8.size() / 4;

    std::vector<float> want( px * 4 );
    auto ref = cmsCreateTransform( in, TYPE_RGBA_8, out, TYPE_RGBA_FLT, IccRenderIntent, cmsFLAGS_COPY_ALPHA );
    REQUIRE( ref );
    cmsDoTransform( ref, u8.data(), want.data(), (cmsUInt32Number)px );
    cmsDeleteTransform( ref );

    auto inNoClut = cmsCreateRGBProfile( &white709, &primaries709, linear3 );
    REQUIRE( inNoClut );
    std::vector<float> matrixOnly( px * 4, 0.f );
    const FastIccTransform fast( inNoClut, out );
    REQUIRE( static_cast<bool>( fast ) );
    REQUIRE( fast.Run( nullptr, u8.data(), matrixOnly.data(), px ) );
    CHECK( MaxDiff( want, matrixOnly ) > 0.05f );
    cmsCloseProfile( inNoClut );

    const IccTransform t( in, out );
    std::vector<float> got( px * 4 );
    REQUIRE( t.Run( nullptr, u8.data(), got.data(), px ) );
    for( size_t i = 0; i < got.size(); i += 4 )
    {
        for( int c = 0; c < 3; c++ )
        {
            CHECK( got[i + c] == Catch::Approx( want[i + c] ).margin( 1e-4f ) );
        }
        CHECK( got[i + 3] == Catch::Approx( want[i + 3] ).margin( 1e-4f ) );
    }

    cmsCloseProfile( in );
    cmsCloseProfile( out );
    cmsFreeToneCurve( linear );
}
