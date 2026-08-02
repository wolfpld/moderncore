#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <contrib/half.hpp>
#include <src/util/Bitmap.hpp>
#include <src/util/BitmapHdr.hpp>
#include <src/util/BitmapHdrHalf.hpp>
#include <src/util/TaskDispatch.hpp>
#include <src/util/Tonemapper.hpp>
#include <array>
#include <functional>
#include <vector>

namespace
{

void SetPixel( BitmapHdr& bmp, uint32_t x, uint32_t y, float r, float g, float b, float a )
{
    float* p = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

std::array<float, 4> GetPixel( const BitmapHdr& bmp, uint32_t x, uint32_t y )
{
    const float* p = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
    return { p[0], p[1], p[2], p[3] };
}

// Distinct color per pixel so flips/rotations are detectable
void FillPattern( BitmapHdr& bmp )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            SetPixel( bmp, x, y, ( x + 1 ) * 0.25f, ( y + 1 ) * 0.125f, ( x + y + 1 ) * 0.03125f, 1.0f );
        }
    }
}

void FillSolid( BitmapHdr& bmp, float r, float g, float b, float a = 1.0f )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            SetPixel( bmp, x, y, r, g, b, a );
        }
    }
}

void VerifySolid( const BitmapHdr& bmp, float r, float g, float b, float a )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            const auto px = GetPixel( bmp, x, y );
            REQUIRE( px[0] == Catch::Approx( r ).margin( 0.001f ) );
            REQUIRE( px[1] == Catch::Approx( g ).margin( 0.001f ) );
            REQUIRE( px[2] == Catch::Approx( b ).margin( 0.001f ) );
            REQUIRE( px[3] == Catch::Approx( a ).margin( 0.001f ) );
        }
    }
}

}

TEST_CASE( "BitmapHdr constructor and accessors", "[bitmaphdr]" )
{
    SECTION( "Direct construction" )
    {
        BitmapHdr bmp( 4, 2, Colorspace::BT709, 3 );
        REQUIRE( bmp.Width() == 4 );
        REQUIRE( bmp.Height() == 2 );
        REQUIRE( bmp.Data() != nullptr );
        REQUIRE( bmp.Orientation() == 3 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );
    }

    SECTION( "BT2020 colorspace" )
    {
        BitmapHdr bmp( 2, 2, Colorspace::BT2020 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT2020 );
        REQUIRE( bmp.Orientation() == 0 );
    }

    SECTION( "Conversion from BitmapHdrHalf" )
    {
        BitmapHdrHalf half( 3, 2, Colorspace::BT2020, 6 );
        for( uint32_t y = 0; y < 2; y++ )
        {
            for( uint32_t x = 0; x < 3; x++ )
            {
                half_float::half* p = half.Data() + ( size_t( y ) * 3 + x ) * 4;
                p[0] = ( x + 1 ) * 0.25f;
                p[1] = ( y + 1 ) * 0.125f;
                p[2] = ( x + y + 1 ) * 0.03125f;
                p[3] = 1.0f;
            }
        }

        BitmapHdr bmp( half );
        REQUIRE( bmp.Width() == 3 );
        REQUIRE( bmp.Height() == 2 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT2020 );
        REQUIRE( bmp.Orientation() == 6 );

        for( uint32_t y = 0; y < 2; y++ )
        {
            for( uint32_t x = 0; x < 3; x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                REQUIRE( px[0] == Catch::Approx( ( x + 1 ) * 0.25f ).margin( 0.01f ) );
                REQUIRE( px[1] == Catch::Approx( ( y + 1 ) * 0.125f ).margin( 0.01f ) );
                REQUIRE( px[2] == Catch::Approx( ( x + y + 1 ) * 0.03125f ).margin( 0.01f ) );
                REQUIRE( px[3] == Catch::Approx( 1.0f ).margin( 0.01f ) );
            }
        }
    }
}

TEST_CASE( "BitmapHdr resize", "[bitmaphdr][resize]" )
{
    SECTION( "Resize preserves solid color" )
    {
        BitmapHdr bmp( 16, 16, Colorspace::BT709 );
        FillSolid( bmp, 0.25f, 0.5f, 0.75f );

        bmp.Resize( 7, 5 );
        REQUIRE( bmp.Width() == 7 );
        REQUIRE( bmp.Height() == 5 );
        VerifySolid( bmp, 0.25f, 0.5f, 0.75f, 1.0f );
    }

    SECTION( "ResizeNew leaves original unchanged" )
    {
        BitmapHdr bmp( 8, 8, Colorspace::BT2020, 2 );
        FillSolid( bmp, 0.1f, 0.2f, 0.3f );

        auto resized = bmp.ResizeNew( 3, 3 );
        REQUIRE( resized != nullptr );
        REQUIRE( resized->Width() == 3 );
        REQUIRE( resized->Height() == 3 );
        REQUIRE( resized->GetColorspace() == Colorspace::BT2020 );
        REQUIRE( resized->Orientation() == 2 );

        REQUIRE( bmp.Width() == 8 );
        REQUIRE( bmp.Height() == 8 );
    }

    SECTION( "Resize with TaskDispatch" )
    {
        BitmapHdr bmp( 12, 12, Colorspace::BT709 );
        FillSolid( bmp, 0.3f, 0.4f, 0.5f );

        TaskDispatch td( 4, "hdr-resize" );
        bmp.Resize( 5, 5, &td );

        REQUIRE( bmp.Width() == 5 );
        REQUIRE( bmp.Height() == 5 );
        VerifySolid( bmp, 0.3f, 0.4f, 0.5f, 1.0f );
    }
}

TEST_CASE( "BitmapHdr crop and alpha", "[bitmaphdr][crop][alpha]" )
{
    SECTION( "Crop preserves selected region" )
    {
        BitmapHdr bmp( 4, 4, Colorspace::BT709 );
        FillPattern( bmp );

        bmp.Crop( 1, 1, 2, 2 );
        REQUIRE( bmp.Width() == 2 );
        REQUIRE( bmp.Height() == 2 );

        for( uint32_t y = 0; y < 2; y++ )
        {
            for( uint32_t x = 0; x < 2; x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                REQUIRE( px[0] == Catch::Approx( ( x + 2 ) * 0.25f ) );
                REQUIRE( px[1] == Catch::Approx( ( y + 2 ) * 0.125f ) );
                REQUIRE( px[2] == Catch::Approx( ( x + y + 3 ) * 0.03125f ) );
                REQUIRE( px[3] == Catch::Approx( 1.0f ) );
            }
        }
    }

    SECTION( "SetAlpha replaces the alpha channel" )
    {
        BitmapHdr bmp( 8, 8, Colorspace::BT709 ); // 64 pixels exercises SIMD paths
        FillSolid( bmp, 0.2f, 0.4f, 0.6f, 1.0f );

        bmp.SetAlpha( 0.5f );
        VerifySolid( bmp, 0.2f, 0.4f, 0.6f, 0.5f );
    }

    SECTION( "SetAlpha to zero" )
    {
        BitmapHdr bmp( 8, 8, Colorspace::BT709 );
        FillSolid( bmp, 0.2f, 0.4f, 0.6f, 1.0f );

        bmp.SetAlpha( 0.0f );
        VerifySolid( bmp, 0.2f, 0.4f, 0.6f, 0.0f );
    }

    SECTION( "Small bitmap exercises scalar tail" )
    {
        BitmapHdr bmp( 1, 2, Colorspace::BT709 );
        FillSolid( bmp, 0.1f, 0.2f, 0.3f, 1.0f );

        bmp.SetAlpha( 0.25f );
        VerifySolid( bmp, 0.1f, 0.2f, 0.3f, 0.25f );
    }
}

TEST_CASE( "BitmapHdr flips and rotations", "[bitmaphdr][transform]" )
{
    SECTION( "FlipVertical" )
    {
        BitmapHdr bmp( 3, 3, Colorspace::BT709 );
        FillPattern( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();
        auto orig = std::vector<std::array<float, 4>>();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                orig.push_back( GetPixel( bmp, x, y ) );
            }
        }

        bmp.FlipVertical();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                const auto ex = orig[size_t( h - 1 - y ) * w + x];
                REQUIRE( px[0] == Catch::Approx( ex[0] ) );
                REQUIRE( px[1] == Catch::Approx( ex[1] ) );
                REQUIRE( px[2] == Catch::Approx( ex[2] ) );
            }
        }
    }

    SECTION( "FlipHorizontal" )
    {
        BitmapHdr bmp( 3, 3, Colorspace::BT709 );
        FillPattern( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();
        auto orig = std::vector<std::array<float, 4>>();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                orig.push_back( GetPixel( bmp, x, y ) );
            }
        }

        bmp.FlipHorizontal();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                const auto ex = orig[size_t( y ) * w + ( w - 1 - x )];
                REQUIRE( px[0] == Catch::Approx( ex[0] ) );
                REQUIRE( px[1] == Catch::Approx( ex[1] ) );
                REQUIRE( px[2] == Catch::Approx( ex[2] ) );
            }
        }
    }

    SECTION( "Rotate90" )
    {
        BitmapHdr bmp( 3, 2, Colorspace::BT709 );
        FillPattern( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();
        auto orig = std::vector<std::array<float, 4>>();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                orig.push_back( GetPixel( bmp, x, y ) );
            }
        }

        bmp.Rotate90();
        REQUIRE( bmp.Width() == h );
        REQUIRE( bmp.Height() == w );
        for( uint32_t y = 0; y < bmp.Height(); y++ )
        {
            for( uint32_t x = 0; x < bmp.Width(); x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                const auto ex = orig[size_t( h - 1 - x ) * w + y];
                REQUIRE( px[0] == Catch::Approx( ex[0] ) );
                REQUIRE( px[1] == Catch::Approx( ex[1] ) );
                REQUIRE( px[2] == Catch::Approx( ex[2] ) );
            }
        }
    }

    SECTION( "Rotate180" )
    {
        BitmapHdr bmp( 3, 2, Colorspace::BT709 );
        FillPattern( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();
        auto orig = std::vector<std::array<float, 4>>();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                orig.push_back( GetPixel( bmp, x, y ) );
            }
        }

        bmp.Rotate180();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                const auto ex = orig[size_t( h - 1 - y ) * w + ( w - 1 - x )];
                REQUIRE( px[0] == Catch::Approx( ex[0] ) );
                REQUIRE( px[1] == Catch::Approx( ex[1] ) );
                REQUIRE( px[2] == Catch::Approx( ex[2] ) );
            }
        }
    }

    SECTION( "Rotate270" )
    {
        BitmapHdr bmp( 3, 2, Colorspace::BT709 );
        FillPattern( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();
        auto orig = std::vector<std::array<float, 4>>();
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                orig.push_back( GetPixel( bmp, x, y ) );
            }
        }

        bmp.Rotate270();
        REQUIRE( bmp.Width() == h );
        REQUIRE( bmp.Height() == w );
        for( uint32_t y = 0; y < bmp.Height(); y++ )
        {
            for( uint32_t x = 0; x < bmp.Width(); x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                const auto ex = orig[size_t( x ) * w + ( w - 1 - y )];
                REQUIRE( px[0] == Catch::Approx( ex[0] ) );
                REQUIRE( px[1] == Catch::Approx( ex[1] ) );
                REQUIRE( px[2] == Catch::Approx( ex[2] ) );
            }
        }
    }

    SECTION( "Four rotations return to original" )
    {
        BitmapHdr bmp( 4, 2, Colorspace::BT709 );
        FillPattern( bmp );
        auto orig = std::vector<std::array<float, 4>>();
        for( uint32_t y = 0; y < bmp.Height(); y++ )
        {
            for( uint32_t x = 0; x < bmp.Width(); x++ )
            {
                orig.push_back( GetPixel( bmp, x, y ) );
            }
        }

        bmp.Rotate90();
        bmp.Rotate90();
        bmp.Rotate90();
        bmp.Rotate90();
        for( uint32_t y = 0; y < bmp.Height(); y++ )
        {
            for( uint32_t x = 0; x < bmp.Width(); x++ )
            {
                const auto px = GetPixel( bmp, x, y );
                const auto ex = orig[size_t( y ) * bmp.Width() + x];
                REQUIRE( px[0] == Catch::Approx( ex[0] ) );
                REQUIRE( px[1] == Catch::Approx( ex[1] ) );
                REQUIRE( px[2] == Catch::Approx( ex[2] ) );
            }
        }
    }
}

TEST_CASE( "BitmapHdr normalize orientation", "[bitmaphdr][orientation]" )
{
    SECTION( "Orientations 0 and 1 are no-ops" )
    {
        for( int orientation : { 0, 1 } )
        {
            BitmapHdr bmp( 3, 3, Colorspace::BT709, orientation );
            FillSolid( bmp, 0.1f, 0.2f, 0.3f );
            bmp.NormalizeOrientation();
            REQUIRE( bmp.Orientation() == orientation );
            VerifySolid( bmp, 0.1f, 0.2f, 0.3f, 1.0f );
        }
    }

    SECTION( "Each orientation applies the documented transform sequence" )
    {
        struct Case
        {
            int orientation;
            std::vector<std::function<void(BitmapHdr&)>> ops;
        };

        const std::vector<Case> cases = {
            { 2, { []( BitmapHdr& b ) { b.FlipHorizontal(); } } },
            { 3, { []( BitmapHdr& b ) { b.Rotate180(); } } },
            { 4, { []( BitmapHdr& b ) { b.FlipVertical(); } } },
            { 5, { []( BitmapHdr& b ) { b.Rotate270(); b.FlipVertical(); } } },
            { 6, { []( BitmapHdr& b ) { b.Rotate90(); } } },
            { 7, { []( BitmapHdr& b ) { b.Rotate90(); b.FlipVertical(); } } },
            { 8, { []( BitmapHdr& b ) { b.Rotate270(); } } },
        };

        for( const auto& testCase : cases )
        {
            BitmapHdr normalized( 4, 3, Colorspace::BT709, testCase.orientation );
            BitmapHdr manual( 4, 3, Colorspace::BT709, testCase.orientation );
            FillPattern( normalized );
            FillPattern( manual );

            normalized.NormalizeOrientation();
            REQUIRE( normalized.Orientation() == 1 );

            for( const auto& op : testCase.ops )
            {
                op( manual );
            }

            REQUIRE( normalized.Width() == manual.Width() );
            REQUIRE( normalized.Height() == manual.Height() );
            for( uint32_t y = 0; y < normalized.Height(); y++ )
            {
                for( uint32_t x = 0; x < normalized.Width(); x++ )
                {
                    const auto pn = GetPixel( normalized, x, y );
                    const auto pm = GetPixel( manual, x, y );
                    REQUIRE( pn[0] == Catch::Approx( pm[0] ) );
                    REQUIRE( pn[1] == Catch::Approx( pm[1] ) );
                    REQUIRE( pn[2] == Catch::Approx( pm[2] ) );
                    REQUIRE( pn[3] == Catch::Approx( pm[3] ) );
                }
            }
        }
    }

    SECTION( "Invalid orientation aborts" )
    {
        BitmapHdr bmp( 2, 2, Colorspace::BT709, 9 );
        REQUIRE_ABORTS( bmp.NormalizeOrientation() );
    }
}

TEST_CASE( "BitmapHdr colorspace transforms", "[bitmaphdr][colorspace]" )
{
    SECTION( "BT2020 to BT709 transforms pixels and preserves alpha" )
    {
        BitmapHdr bmp( 8, 8, Colorspace::BT2020 );
        FillSolid( bmp, 0.2f, 0.5f, 0.8f, 0.75f );

        bmp.SetColorspace( Colorspace::BT709 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );

        // Alpha preserved exactly, RGB transformed
        const auto px = GetPixel( bmp, 0, 0 );
        REQUIRE( px[3] == Catch::Approx( 0.75f ) );
        REQUIRE( px[0] != Catch::Approx( 0.2f ) );
    }

    SECTION( "BT709 to BT2020 transforms pixels" )
    {
        BitmapHdr bmp( 8, 8, Colorspace::BT709 );
        FillSolid( bmp, 0.2f, 0.5f, 0.8f, 1.0f );

        bmp.SetColorspace( Colorspace::BT2020 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT2020 );

        const auto px = GetPixel( bmp, 0, 0 );
        REQUIRE( px[3] == Catch::Approx( 1.0f ) );
        REQUIRE( px[0] != Catch::Approx( 0.2f ) );
    }

    SECTION( "No-op transform leaves data unchanged" )
    {
        BitmapHdr bmp( 4, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.3f, 0.4f, 0.5f );

        bmp.SetColorspace( Colorspace::BT709 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );
        VerifySolid( bmp, 0.3f, 0.4f, 0.5f, 1.0f );
    }

    SECTION( "Transform with TaskDispatch" )
    {
        BitmapHdr bmp( 16, 16, Colorspace::BT2020 );
        FillSolid( bmp, 0.1f, 0.3f, 0.6f, 0.5f );

        TaskDispatch td( 4, "hdr-colorspace" );
        bmp.SetColorspace( Colorspace::BT709, &td );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );

        const auto px = GetPixel( bmp, 5, 5 );
        REQUIRE( px[3] == Catch::Approx( 0.5f ) );
        REQUIRE( px[0] != Catch::Approx( 0.1f ) );
    }
}

TEST_CASE( "BitmapHdr tonemap", "[bitmaphdr][tonemap]" )
{
    SECTION( "Tonemap produces an 8-bit bitmap of matching size" )
    {
        BitmapHdr bmp( 6, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.5f, 0.6f, 0.7f, 1.0f );

        auto result = bmp.Tonemap( ToneMap::Operator::AgX );
        REQUIRE( result != nullptr );
        REQUIRE( result->Width() == 6 );
        REQUIRE( result->Height() == 4 );
    }

    SECTION( "All operators produce valid output" )
    {
        BitmapHdr bmp( 4, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.4f, 0.5f, 0.6f, 1.0f );

        for( ToneMap::Operator op : { ToneMap::Operator::AgX, ToneMap::Operator::AgXGolden, ToneMap::Operator::AgXPunchy, ToneMap::Operator::PbrNeutral } )
        {
            auto result = bmp.Tonemap( op );
            REQUIRE( result != nullptr );
            REQUIRE( result->Width() == 4 );
            REQUIRE( result->Height() == 4 );

            // Alpha channel is preserved
            const auto px = *(uint32_t*)result->Data();
            REQUIRE( ( px >> 24 ) == 255 );
        }
    }

    SECTION( "Tonemap on non-BT709 colorspace aborts" )
    {
        BitmapHdr bmp( 2, 2, Colorspace::BT2020 );
        REQUIRE_ABORTS( bmp.Tonemap( ToneMap::Operator::AgX ) );
    }
}

TEST_CASE( "BitmapHdr panic paths abort", "[bitmaphdr][panic]" )
{
    SECTION( "Crop out of bounds" )
    {
        BitmapHdr bmp( 3, 3, Colorspace::BT709 );
        REQUIRE_ABORTS( bmp.Crop( 2, 2, 2, 2 ) );
    }
}
