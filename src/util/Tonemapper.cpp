#include <algorithm>
#include <cmath>

#include "Simd.hpp"
#include "Tonemapper.hpp"

namespace ToneMap
{

namespace
{
struct HdrColor
{
    float r;
    float g;
    float b;
    float a;
};

HdrColor PbrNeutral( const HdrColor& hdr )
{
    constexpr auto startCompression = 0.8f - 0.04f;
    constexpr auto desaturation = 0.15f;
    constexpr auto d = 1.f - startCompression;

    const auto x = std::min( { hdr.r, hdr.g, hdr.b } );
    const auto offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;

    auto color = HdrColor { hdr.r - offset, hdr.g - offset, hdr.b - offset };

    const auto peak = std::max( { color.r, color.g, color.b } );
    if( peak < startCompression ) return color;

    const auto newPeak = 1.f - d * d / ( peak + d - startCompression );
    color.r *= newPeak / peak;
    color.g *= newPeak / peak;
    color.b *= newPeak / peak;

    const auto g = 1.f - 1.f / ( desaturation * ( peak - newPeak ) + 1.f );

    return {
        std::lerp( color.r, newPeak, g ),
        std::lerp( color.g, newPeak, g ),
        std::lerp( color.b, newPeak, g ),
    };
}

float LinearToSrgb( float x )
{
    if( x <= 0.0031308f ) return 12.92f * x;
    return 1.055f * std::pow( x, 1.0f / 2.4f ) - 0.055f;
}
}

void PbrNeutral( uint32_t* dst, float* src, size_t sz )
{
#if defined __SSE4_1__ && defined __FMA__
#if defined __AVX2__
    while( sz > 1 )
    {
        const HdrColor color[2] = {
            PbrNeutral( { src[0], src[1], src[2] } ),
            PbrNeutral( { src[4], src[5], src[6] } )
        };

        __m256 s0 = _mm256_loadu_ps( src );
        __m256 v0 = _mm256_loadu_ps( (const float*)color );
        __m256 v1 = _mm256_cmp_ps( v0, _mm256_set1_ps( 0.0031308f ), _CMP_LE_OQ );
        __m256 v2 = _mm256_mul_ps( v0, _mm256_set1_ps( 12.92f ) );
        __m256 v3 = _mm256_pow_ps( v0, _mm256_set1_ps( 1.0f / 2.4f ) );
        __m256 v4 = _mm256_mul_ps( v3, _mm256_set1_ps( 1.055f ) );
        __m256 v5 = _mm256_sub_ps( v4, _mm256_set1_ps( 0.055f ) );
        __m256 v6 = _mm256_blendv_ps( v5, v2, v1 );
        __m256 v7 = _mm256_blend_ps( v6, s0, 0x88 );
        __m256 v8 = _mm256_min_ps( v7, _mm256_set1_ps( 1.0f ) );
        __m256 v9 = _mm256_max_ps( v8, _mm256_set1_ps( 0.0f ) );
        __m256 v10 = _mm256_mul_ps( v9, _mm256_set1_ps( 255.0f ) );
        __m256i v11 = _mm256_cvtps_epi32( v10 );
        __m256i v12 = _mm256_packus_epi32( v11, v11 );
        __m256i v13 = _mm256_packus_epi16( v12, v12 );
        *dst++ = _mm_cvtsi128_si32( _mm256_castsi256_si128( v13 ) );
        *dst++ = _mm_cvtsi128_si32( _mm256_extracti128_si256( v13, 1 ) );

        src += 8;
        sz -= 2;
    }
#endif
    while( sz > 0 )
    {
        const auto color = PbrNeutral( { src[0], src[1], src[2] } );

        __m128 s0 = _mm_loadu_ps( src );
        __m128 v0 = _mm_loadu_ps( &color.r );
        __m128 v1 = _mm_cmple_ps( v0, _mm_set1_ps( 0.0031308f ) );
        __m128 v2 = _mm_mul_ps( v0, _mm_set1_ps( 12.92f ) );
        __m128 v3 = _mm_pow_ps( v0, _mm_set1_ps( 1.0f / 2.4f ) );
        __m128 v4 = _mm_mul_ps( v3, _mm_set1_ps( 1.055f ) );
        __m128 v5 = _mm_sub_ps( v4, _mm_set1_ps( 0.055f ) );
        __m128 v6 = _mm_blendv_ps( v5, v2, v1 );
        __m128 v7 = _mm_blend_ps( v6, s0, 0x8 );
        __m128 v8 = _mm_min_ps( v7, _mm_set1_ps( 1.0f ) );
        __m128 v9 = _mm_max_ps( v8, _mm_set1_ps( 0.0f ) );
        __m128 v10 = _mm_mul_ps( v9, _mm_set1_ps( 255.0f ) );
        __m128i v11 = _mm_cvtps_epi32( v10 );
        __m128i v12 = _mm_packus_epi32( v11, v11 );
        __m128i v13 = _mm_packus_epi16( v12, v12 );
        *dst++ = _mm_cvtsi128_si32( v13 );

        src += 4;
        sz--;
    }
#else
    do
    {
        const auto color = PbrNeutral( { src[0], src[1], src[2] } );

        const auto r = LinearToSrgb( color.r );
        const auto g = LinearToSrgb( color.g );
        const auto b = LinearToSrgb( color.b );
        const auto a = src[3];

        *dst++ = (uint32_t( std::clamp( a, 0.0f, 1.0f ) * 255.0f ) << 24) |
                 (uint32_t( std::clamp( b, 0.0f, 1.0f ) * 255.0f ) << 16) |
                 (uint32_t( std::clamp( g, 0.0f, 1.0f ) * 255.0f ) << 8) |
                  uint32_t( std::clamp( r, 0.0f, 1.0f ) * 255.0f );

        src += 4;
    }
    while( --sz );
#endif
}

}
