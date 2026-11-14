#pragma once

#include <algorithm>
#include <math.h>
#include <stdint.h>

#include "Simd.hpp"

// SMPTE ST 2084 (PQ) inverse EOTF. N is the normalized code value (0..1 for
// 0..10000 cd/m2); the result is cd/m2 multiplied by NominalLuminanceMul, so
// 1.f / 203.f yields BitmapHdr units (1.0 == SDR reference white).
static inline float Pq( float N, float NominalLuminanceMul )
{
    constexpr float m1 = 0.1593017578125f;
    constexpr float m1inv = 1.f / m1;
    constexpr float m2 = 78.84375f;
    constexpr float m2inv = 1.f / m2;
    constexpr float c1 = 0.8359375f;
    constexpr float c2 = 18.8515625f;
    constexpr float c3 = 18.6875f;

    const auto Nm2 = std::powf( std::max( N, 0.f ), m2inv );
    return 10000.f * std::powf( std::max( 0.f, Nm2 - c1 ) / ( c2 - c3 * Nm2 ), m1inv ) * NominalLuminanceMul;
}

// ARIB STD-B67 (HLG) OETF inverse, BT.2100 normalized for scene light 0..1.
static inline float Hlg( float E )
{
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;

    if( E < 0 )
    {
        return -1.f * Hlg( -1.f * E );
    }
    else if( E <= 0.5f )
    {
        return E * E / 3.f;
    }
    else
    {
        return ( std::exp( ( E - c ) / a ) + b ) / 12.f;
    }
}

#if defined __SSE4_1__ && defined __FMA__
static inline void LinearizePq128( float* ptr, size_t sz, float NominalLuminanceMul )
{
    while( sz > 0 )
    {
        __m128 px0 = _mm_loadu_ps( ptr );
        __m128 px1 = _mm_max_ps( px0, _mm_setzero_ps() );
        __m128 Nm2 = _mm_pow_ps( px1, _mm_set1_ps( 1.f / 78.84375f ) );

        __m128 px2 = _mm_sub_ps( Nm2, _mm_set1_ps( 0.8359375f ) );
        __m128 px3 = _mm_max_ps( px2, _mm_setzero_ps() );

        __m128 px4 = _mm_fnmadd_ps( _mm_set1_ps( 18.6875f ), Nm2, _mm_set1_ps( 18.8515625f ) );
        __m128 px5 = _mm_div_ps( px3, px4 );

        __m128 px6 = _mm_pow_ps( px5, _mm_set1_ps( 1.f / 0.1593017578125f ) );
        __m128 ret = _mm_mul_ps( px6, _mm_set1_ps( 10000.f * NominalLuminanceMul ) );

        __m128 b = _mm_blend_ps( ret, px0, 0x8 );
        _mm_storeu_ps( ptr, b );

        ptr += 4;
        sz--;
    }
}

#  if defined __AVX2__
static inline void LinearizePq256( float* ptr, size_t sz, float NominalLuminanceMul )
{
    while( sz > 1 )
    {
        __m256 px0 = _mm256_loadu_ps( ptr );
        __m256 px1 = _mm256_max_ps( px0, _mm256_setzero_ps() );
        __m256 Nm2 = _mm256_pow_ps( px1, _mm256_set1_ps( 1.f / 78.84375f ) );

        __m256 px2 = _mm256_sub_ps( Nm2, _mm256_set1_ps( 0.8359375f ) );
        __m256 px3 = _mm256_max_ps( px2, _mm256_setzero_ps() );

        __m256 px4 = _mm256_fnmadd_ps( _mm256_set1_ps( 18.6875f ), Nm2, _mm256_set1_ps( 18.8515625f ) );
        __m256 px5 = _mm256_div_ps( px3, px4 );

        __m256 px6 = _mm256_pow_ps( px5, _mm256_set1_ps( 1.f / 0.1593017578125f ) );
        __m256 ret = _mm256_mul_ps( px6, _mm256_set1_ps( 10000.f * NominalLuminanceMul ) );

        __m256 b = _mm256_blend_ps( ret, px0, 0x88 );
        _mm256_storeu_ps( ptr, b );

        ptr += 8;
        sz -= 2;
    }
}
#  endif

#  if defined __AVX512F__
static inline void LinearizePq512( float* ptr, size_t sz, float NominalLuminanceMul )
{
    while( sz > 3 )
    {
        __m512 px0 = _mm512_loadu_ps( ptr );
        __m512 px1 = _mm512_max_ps( px0, _mm512_setzero_ps() );
        __m512 Nm2 = _mm512_pow_ps( px1, _mm512_set1_ps( 1.f / 78.84375f ) );

        __m512 px2 = _mm512_sub_ps( Nm2, _mm512_set1_ps( 0.8359375f ) );
        __m512 px3 = _mm512_max_ps( px2, _mm512_setzero_ps() );

        __m512 px4 = _mm512_fnmadd_ps( _mm512_set1_ps( 18.6875f ), Nm2, _mm512_set1_ps( 18.8515625f ) );
        __m512 px5 = _mm512_div_ps( px3, px4 );

        __m512 px6 = _mm512_pow_ps( px5, _mm512_set1_ps( 1.f / 0.1593017578125f ) );
        __m512 ret = _mm512_mul_ps( px6, _mm512_set1_ps( 10000.f * NominalLuminanceMul ) );

        __m512 b = _mm512_mask_blend_ps( 0x8888, ret, px0 );
        _mm512_storeu_ps( ptr, b );

        ptr += 16;
        sz -= 4;
    }
}
#  endif

static inline void LinearizePq( float* ptr, size_t sz, float NominalLuminanceMul )
{
#  ifdef __AVX512F__
    LinearizePq512( ptr, sz, NominalLuminanceMul );
    if( (sz & 3) == 0 ) return;
    ptr += ( sz & ~3 ) * 4;
    sz &= 3;
#  endif
#  ifdef __AVX2__
    LinearizePq256( ptr, sz, NominalLuminanceMul );
    if( (sz & 1) == 0 ) return;
    ptr += ( sz & ~1 ) * 4;
    sz &= 1;
#  endif
    LinearizePq128( ptr, sz, NominalLuminanceMul );
}
#else
static inline void LinearizePq( float* ptr, size_t sz, float NominalLuminanceMul )
{
    for( size_t i=0; i<sz; i++ )
    {
        ptr[0] = Pq( ptr[0], NominalLuminanceMul );
        ptr[1] = Pq( ptr[1], NominalLuminanceMul );
        ptr[2] = Pq( ptr[2], NominalLuminanceMul );

        ptr += 4;
    }
}
#endif

// BT.2100 HLG display light: OETF inverse, luma-based system gamma, and
// 1000 cd/m2 nominal peak scaled to BitmapHdr units. In-place over sz pixels.
static inline void LinearizeHlg( float* ptr, size_t sz )
{
    while( sz-- )
    {
        const auto r = Hlg( ptr[0] );
        const auto g = Hlg( ptr[1] );
        const auto b = Hlg( ptr[2] );
        const auto ys = std::powf( std::max( 0.f, 0.2627f * r + 0.6780f * g + 0.0593f * b ), 0.2f );
        const float scale = 1000.f / 203.f;

        ptr[0] = r * ys * scale;
        ptr[1] = g * ys * scale;
        ptr[2] = b * ys * scale;

        ptr += 4;
    }
}
