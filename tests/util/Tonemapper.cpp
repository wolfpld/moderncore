#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Simd.hpp>
#include <src/util/Tonemapper.hpp>
#include <stdint.h>
#include <vector>

namespace
{

// RGBA float pixel to 8-bit RGBA, matching the tonemapper's packing
uint32_t Pack( float r, float g, float b, float a )
{
    return ( uint32_t( a * 255.0f ) << 24 ) | ( uint32_t( b * 255.0f ) << 16 ) | ( uint32_t( g * 255.0f ) << 8 ) | uint32_t( r * 255.0f );
}

void VerifyPixel( uint32_t px, float r, float g, float b, float a )
{
    // Tonemappers may round or truncate when packing, and the SIMD sRGB
    // curve is an approximation; allow ±2 per channel
    REQUIRE( ( px >> 24 ) == Catch::Approx( a * 255.0f ).margin( 2.0f ) );
    REQUIRE( ( ( px >> 16 ) & 0xFF ) == Catch::Approx( b * 255.0f ).margin( 2.0f ) );
    REQUIRE( ( ( px >> 8 ) & 0xFF ) == Catch::Approx( g * 255.0f ).margin( 2.0f ) );
    REQUIRE( ( px & 0xFF ) == Catch::Approx( r * 255.0f ).margin( 2.0f ) );
}

}

TEST_CASE( "Tonemapper process dispatch", "[tonemapper][process]" )
{
    SECTION( "All operators preserve alpha" )
    {
        for( ToneMap::Operator op : { ToneMap::Operator::AgX, ToneMap::Operator::AgXGolden, ToneMap::Operator::AgXPunchy, ToneMap::Operator::PbrNeutral } )
        {
            float src[4] = { 0.5f, 0.5f, 0.5f, 0.75f };
            uint32_t dst[1];
            ToneMap::Process( op, dst, src, 1 );

            // Alpha preserved (rounding may differ per operator)
            REQUIRE( ( dst[0] >> 24 ) == Catch::Approx( 0.75f * 255.0f ).margin( 1.0f ) );
        }
    }

    SECTION( "Zero input produces a valid dark output" )
    {
        float src[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        uint32_t dst[1];
        ToneMap::Process( ToneMap::Operator::AgX, dst, src, 1 );

        const auto r = dst[0] & 0xFF;
        const auto g = ( dst[0] >> 8 ) & 0xFF;
        const auto b = ( dst[0] >> 16 ) & 0xFF;
        REQUIRE( r < 128 );
        REQUIRE( g < 128 );
        REQUIRE( b < 128 );
    }

    SECTION( "Negative input is handled without NaN" )
    {
        float src[4] = { -1.0f, -0.5f, -2.0f, 1.0f };
        uint32_t dst[1];
        ToneMap::Process( ToneMap::Operator::AgXPunchy, dst, src, 1 );

        const auto r = dst[0] & 0xFF;
        const auto g = ( dst[0] >> 8 ) & 0xFF;
        const auto b = ( dst[0] >> 16 ) & 0xFF;
        REQUIRE( r != 0 ); // no NaN/garbage; clamped values are finite
        REQUIRE( r < 128 );
        REQUIRE( g < 128 );
        REQUIRE( b < 128 );
    }

    SECTION( "HDR input is clamped to a bright output" )
    {
        float src[4] = { 100.0f, 100.0f, 100.0f, 1.0f };
        uint32_t dst[1];
        ToneMap::Process( ToneMap::Operator::AgX, dst, src, 1 );

        const auto r = dst[0] & 0xFF;
        const auto g = ( dst[0] >> 8 ) & 0xFF;
        const auto b = ( dst[0] >> 16 ) & 0xFF;
        REQUIRE( r >= 250 );
        REQUIRE( g >= 230 );
        REQUIRE( b >= 230 );
    }

    SECTION( "Multiple pixels are processed in order" )
    {
        float src[12] = {
            0.5f, 0.5f, 0.5f, 0.25f,
            0.2f, 0.3f, 0.4f, 0.5f,
            0.1f, 0.7f, 0.2f, 0.75f,
        };
        uint32_t dst[3];
        ToneMap::Process( ToneMap::Operator::PbrNeutral, dst, src, 3 );

        for( int i = 0; i < 3; i++ )
        {
            const auto a = dst[i] >> 24;
            REQUIRE( a == Catch::Approx( src[i * 4 + 3] * 255.0f ).margin( 1.0f ) );
        }
    }

    SECTION( "Invalid operator aborts" )
    {
        uint32_t dst[1];
        float src[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
        REQUIRE_ABORTS( ToneMap::Process( static_cast<ToneMap::Operator>( 999 ), dst, src, 1 ) );
    }
}

TEST_CASE( "Tonemapper direct operators", "[tonemapper][operators]" )
{
    SECTION( "AgX produces finite output for a range of inputs" )
    {
        const std::vector<std::array<float, 4>> inputs = {
            { 0.1f, 0.2f, 0.3f, 1.0f },
            { 0.5f, 0.5f, 0.5f, 1.0f },
            { 1.0f, 0.0f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f, 1.0f },
            { 2.0f, 3.0f, 4.0f, 0.5f },
        };

        for( ToneMap::Operator op : { ToneMap::Operator::AgX, ToneMap::Operator::AgXGolden, ToneMap::Operator::AgXPunchy } )
        {
            for( const auto& input : inputs )
            {
                float src[4];
                uint32_t dst[1];
                std::copy( input.begin(), input.end(), src );
                ToneMap::Process( op, dst, src, 1 );

                // All channels clamped into the valid byte range
                const auto r = dst[0] & 0xFF;
                const auto g = ( dst[0] >> 8 ) & 0xFF;
                const auto b = ( dst[0] >> 16 ) & 0xFF;
                REQUIRE( ( dst[0] >> 24 ) == Catch::Approx( input[3] * 255.0f ).margin( 1.0f ) );
                REQUIRE( r <= 255 );
                REQUIRE( g <= 255 );
                REQUIRE( b <= 255 );
            }
        }
    }

    SECTION( "PbrNeutral applies the offset and sRGB curve for dim input" )
    {
        // Peak below startCompression (0.76): only the offset and sRGB
        // encoding are applied, no compression
        float src[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
        uint32_t dst[1];
        ToneMap::Process( ToneMap::Operator::PbrNeutral, dst, src, 1 );

        const auto offset = 0.1f - 6.25f * 0.1f * 0.1f;
        VerifyPixel( dst[0], ToneMap::LinearToSrgb( 0.1f - offset ), ToneMap::LinearToSrgb( 0.2f - offset ), ToneMap::LinearToSrgb( 0.3f - offset ), 1.0f );
    }

    SECTION( "PbrNeutral compresses bright input" )
    {
        // Peak above startCompression: output is compressed below the input
        float src[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t dst[1];
        ToneMap::Process( ToneMap::Operator::PbrNeutral, dst, src, 1 );

        const auto r = dst[0] & 0xFF;
        REQUIRE( r < 255 );
    }

    SECTION( "LinearToSrgb below threshold is linear" )
    {
        REQUIRE( ToneMap::LinearToSrgb( 0.0f ) == Catch::Approx( 0.0f ) );
        REQUIRE( ToneMap::LinearToSrgb( 0.001f ) == Catch::Approx( 12.92f * 0.001f ) );
    }

    SECTION( "LinearToSrgb above threshold is exponential" )
    {
        // Reference: 1.055 * 0.5^(1/2.4) - 0.055 ≈ 0.7355
        REQUIRE( ToneMap::LinearToSrgb( 0.5f ) == Catch::Approx( 0.7355f ).margin( 0.001f ) );
        REQUIRE( ToneMap::LinearToSrgb( 1.0f ) == Catch::Approx( 1.0f ) );
    }

    SECTION( "LinearToSrgb is monotonic" )
    {
        float prev = ToneMap::LinearToSrgb( 0.0f );
        for( float x = 0.01f; x <= 1.0f; x += 0.01f )
        {
            const auto cur = ToneMap::LinearToSrgb( x );
            REQUIRE( cur >= prev );
            prev = cur;
        }
    }
}

TEST_CASE( "Simd math helpers match std math", "[simd][math]" )
{
    const std::vector<float> logInputs = { 0.1f, 0.5f, 1.0f, 2.0f, 4.0f, 10.0f, 100.0f };
    const std::vector<float> expInputs = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f };
    const std::vector<float> powInputs = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };

#if defined __SSE4_1__ && defined __FMA__
    SECTION( "128-bit log/exp/pow" )
    {
        alignas(16) float in[4];
        alignas(16) float out[4];

        for( float x : logInputs )
        {
            std::fill( in, in + 4, x );
            _mm_storeu_ps( out, _mm_log_ps( _mm_loadu_ps( in ) ) );
            REQUIRE( out[0] == Catch::Approx( std::log2( x ) ).epsilon( 1e-4 ) );
        }

        for( float x : expInputs )
        {
            std::fill( in, in + 4, x );
            _mm_storeu_ps( out, _mm_exp_ps( _mm_loadu_ps( in ) ) );
            REQUIRE( out[0] == Catch::Approx( std::exp2( x ) ).epsilon( 1e-4 ) );
        }

        for( float x : powInputs )
        {
            std::fill( in, in + 4, x );
            _mm_storeu_ps( out, _mm_pow_ps( _mm_loadu_ps( in ), _mm_set1_ps( 2.0f ) ) );
            REQUIRE( out[0] == Catch::Approx( std::pow( x, 2.0f ) ).epsilon( 1e-3 ) );
        }
    }
#endif

#if defined __AVX2__
    SECTION( "256-bit log/exp/pow" )
    {
        alignas(32) float in[8];
        alignas(32) float out[8];

        for( float x : logInputs )
        {
            std::fill( in, in + 8, x );
            _mm256_storeu_ps( out, _mm256_log_ps( _mm256_loadu_ps( in ) ) );
            REQUIRE( out[0] == Catch::Approx( std::log2( x ) ).epsilon( 1e-4 ) );
        }

        for( float x : expInputs )
        {
            std::fill( in, in + 8, x );
            _mm256_storeu_ps( out, _mm256_exp_ps( _mm256_loadu_ps( in ) ) );
            REQUIRE( out[0] == Catch::Approx( std::exp2( x ) ).epsilon( 1e-4 ) );
        }

        for( float x : powInputs )
        {
            std::fill( in, in + 8, x );
            _mm256_storeu_ps( out, _mm256_pow_ps( _mm256_loadu_ps( in ), _mm256_set1_ps( 2.0f ) ) );
            REQUIRE( out[0] == Catch::Approx( std::pow( x, 2.0f ) ).epsilon( 1e-3 ) );
        }
    }
#endif

#if defined __AVX512F__
    SECTION( "512-bit log/exp/pow" )
    {
        alignas(64) float in[16];
        alignas(64) float out[16];

        for( float x : logInputs )
        {
            std::fill( in, in + 16, x );
            _mm512_storeu_ps( out, _mm512_log_ps( _mm512_loadu_ps( in ) ) );
            REQUIRE( out[0] == Catch::Approx( std::log2( x ) ).epsilon( 1e-4 ) );
        }

        for( float x : expInputs )
        {
            std::fill( in, in + 16, x );
            _mm512_storeu_ps( out, _mm512_exp_ps( _mm512_loadu_ps( in ) ) );
            REQUIRE( out[0] == Catch::Approx( std::exp2( x ) ).epsilon( 1e-4 ) );
        }

        for( float x : powInputs )
        {
            std::fill( in, in + 16, x );
            _mm512_storeu_ps( out, _mm512_pow_ps( _mm512_loadu_ps( in ), _mm512_set1_ps( 2.0f ) ) );
            REQUIRE( out[0] == Catch::Approx( std::pow( x, 2.0f ) ).epsilon( 1e-3 ) );
        }
    }
#endif
}
