// Based on https://iolite-engine.com/blog_posts/minimal_agx_implementation

#include <algorithm>
#include <cmath>

#include "Tonemapper.hpp"

namespace ToneMap
{

float AgxDefaultContrast( float x )
{
    const auto x2 = x * x;
    const auto x4 = x2 * x2;

    return + 15.5f      * x4 * x2
           - 40.14f     * x4 * x
           + 31.96f     * x4
           - 6.868f     * x2 * x
           + 0.4298f    * x2
           + 0.1191f    * x
           - 0.00232f;
}

HdrColor AgxTransform( const HdrColor& hdr )
{
    constexpr auto min_ev = -12.47393f;
    constexpr auto max_ev = 4.026069f;

    HdrColor color = {
        0.842479062253094f * hdr.r + 0.0423282422610123f * hdr.g + 0.0423756549057051f * hdr.b,
        0.0784335999999992f * hdr.r + 0.878468636469772f * hdr.g + 0.0784336f * hdr.b,
        0.0792237451477643f * hdr.r + 0.0791661274605434f * hdr.g + 0.879142973793104f * hdr.b,
        hdr.a
    };

    color = {
        color.r <= 0 ? min_ev : std::clamp( std::log2( color.r ), min_ev, max_ev ),
        color.g <= 0 ? min_ev : std::clamp( std::log2( color.g ), min_ev, max_ev ),
        color.b <= 0 ? min_ev : std::clamp( std::log2( color.b ), min_ev, max_ev ),
        color.a
    };

    color = {
        ( color.r - min_ev ) / ( max_ev - min_ev ),
        ( color.g - min_ev ) / ( max_ev - min_ev ),
        ( color.b - min_ev ) / ( max_ev - min_ev ),
        color.a
    };

    color = {
        AgxDefaultContrast( color.r ),
        AgxDefaultContrast( color.g ),
        AgxDefaultContrast( color.b ),
        color.a
    };

    return color;
}

HdrColor AgxLookGolden( const HdrColor& color )
{
    const auto luma = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
    const auto vr = std::pow( std::max( 0.f, color.r ), 0.8f );
    const auto vg = std::pow( std::max( 0.f, color.g * 0.9f ), 0.8f );
    const auto vb = std::pow( std::max( 0.f, color.b * 0.5f ), 0.8f );
    return HdrColor {
        luma + 0.8f * ( vr - luma ),
        luma + 0.8f * ( vg - luma ),
        luma + 0.8f * ( vb - luma ),
        color.a
    };
}

HdrColor AgxLookPunchy( const HdrColor& color )
{
    const auto luma = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
    const auto vr = std::pow( std::max( 0.f, color.r ), 1.35f );
    const auto vg = std::pow( std::max( 0.f, color.g ), 1.35f );
    const auto vb = std::pow( std::max( 0.f, color.b ), 1.35f );
    return HdrColor {
        luma + 1.4f * ( vr - luma ),
        luma + 1.4f * ( vg - luma ),
        luma + 1.4f * ( vb - luma ),
        color.a
    };
}

HdrColor AgxEotf( const HdrColor& color )
{
    HdrColor val = {
        1.19687900512017f * color.r - 0.0528968517574562f * color.g - 0.0529716355144438f * color.b,
        -0.0980208811401368f * color.r + 1.15190312990417f * color.g - 0.0980434501171241f * color.b,
        -0.0990297440797205f * color.r - 0.0989611768448433f * color.g + 1.15107367264116f * color.b,
        color.a
    };

    return HdrColor {
        std::pow( std::max( 0.f, val.r ), 2.2f ),
        std::pow( std::max( 0.f, val.g ), 2.2f ),
        std::pow( std::max( 0.f, val.b ), 2.2f ),
        val.a
    };
}

void AgX( uint32_t* dst, float* src, size_t sz )
{
    do
    {
        auto color = AgxTransform( { src[0], src[1], src[2] } );
        color = AgxEotf( color );

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
}

void AgXGolden( uint32_t* dst, float* src, size_t sz )
{
    do
    {
        auto color = AgxTransform( { src[0], src[1], src[2] } );
        color = AgxLookGolden( color );
        color = AgxEotf( color );

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
}

void AgXPunchy( uint32_t* dst, float* src, size_t sz )
{
    do
    {
        auto color = AgxTransform( { src[0], src[1], src[2] } );
        color = AgxLookPunchy( color );
        color = AgxEotf( color );

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
}

}
