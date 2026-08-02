#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Bitmap.hpp>
#include <src/util/TaskDispatch.hpp>
#include <stdint.h>
#include <string.h>

namespace
{

uint32_t MakePixel( uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF )
{
    return ( uint32_t( a ) << 24 ) | ( uint32_t( b ) << 16 ) | ( uint32_t( g ) << 8 ) | r;
}

void SetPixel( Bitmap& bmp, uint32_t x, uint32_t y, uint32_t value )
{
    uint32_t* p = (uint32_t*)( bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4 );
    *p = value;
}

uint32_t GetPixel( const Bitmap& bmp, uint32_t x, uint32_t y )
{
    return *(uint32_t*)( bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4 );
}

// Distinct color per pixel so flips/rotations are detectable
void FillPattern( Bitmap& bmp )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            const auto r = uint8_t( ( x * 7 + y * 3 ) & 0xFF );
            const auto g = uint8_t( ( x * 5 + y * 11 ) & 0xFF );
            const auto b = uint8_t( ( x * 13 + y * 17 ) & 0xFF );
            SetPixel( bmp, x, y, MakePixel( r, g, b ) );
        }
    }
}

// Fill every pixel with a single color
void FillSolid( Bitmap& bmp, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF )
{
    const auto value = MakePixel( r, g, b, a );
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            SetPixel( bmp, x, y, value );
        }
    }
}

// Copy pixels from src into a freshly constructed bitmap of the same size
std::vector<uint32_t> Snapshot( const Bitmap& bmp )
{
    std::vector<uint32_t> result( size_t( bmp.Width() ) * bmp.Height() );
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            result[size_t( y ) * bmp.Width() + x] = GetPixel( bmp, x, y );
        }
    }
    return result;
}

void VerifyAll( const Bitmap& bmp, uint32_t value )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            REQUIRE( GetPixel( bmp, x, y ) == value );
        }
    }
}

}

TEST_CASE( "Bitmap constructor and accessors", "[bitmap]" )
{
    SECTION( "Default orientation" )
    {
        Bitmap bmp( 4, 2 );
        REQUIRE( bmp.Width() == 4 );
        REQUIRE( bmp.Height() == 2 );
        REQUIRE( bmp.Orientation() == 0 );
        REQUIRE( bmp.Data() != nullptr );
    }

    SECTION( "Explicit orientation" )
    {
        Bitmap bmp( 3, 5, 6 );
        REQUIRE( bmp.Width() == 3 );
        REQUIRE( bmp.Height() == 5 );
        REQUIRE( bmp.Orientation() == 6 );
    }

    SECTION( "Const data access" )
    {
        Bitmap bmp( 2, 2 );
        const Bitmap& ref = bmp;
        REQUIRE( ref.Data() != nullptr );
        REQUIRE( ref.Data() == bmp.Data() );
    }
}

TEST_CASE( "Bitmap move semantics", "[bitmap][move]" )
{
    SECTION( "Move constructor transfers data" )
    {
        Bitmap bmp( 3, 2, 2 );
        FillPattern( bmp );
        auto snapshot = Snapshot( bmp );

        Bitmap moved( std::move( bmp ) );
        REQUIRE( moved.Width() == 3 );
        REQUIRE( moved.Height() == 2 );
        REQUIRE( moved.Orientation() == 2 );
        REQUIRE( moved.Data() != nullptr );
        REQUIRE( Snapshot( moved ) == snapshot );

        // Moved-from object must not free the transferred data
        REQUIRE( bmp.Data() == nullptr );
    }

    SECTION( "Move assignment swaps data" )
    {
        Bitmap src( 2, 2, 4 );
        FillPattern( src );
        auto snapshot = Snapshot( src );

        Bitmap dst( 5, 5 );
        dst = std::move( src );
        REQUIRE( dst.Width() == 2 );
        REQUIRE( dst.Height() == 2 );
        REQUIRE( dst.Orientation() == 4 );
        REQUIRE( Snapshot( dst ) == snapshot );
    }

    SECTION( "Move assignment onto self" )
    {
        Bitmap bmp( 2, 2 );
        bmp = std::move( bmp );
        REQUIRE( bmp.Width() == 2 );
        REQUIRE( bmp.Height() == 2 );
    }
}

TEST_CASE( "Bitmap resize", "[bitmap][resize]" )
{
    SECTION( "Resize preserves solid color" )
    {
        Bitmap bmp( 16, 16 );
        FillSolid( bmp, 0x12, 0x34, 0x56 );

        bmp.Resize( 7, 5 );
        REQUIRE( bmp.Width() == 7 );
        REQUIRE( bmp.Height() == 5 );
        VerifyAll( bmp, MakePixel( 0x12, 0x34, 0x56 ) );
    }

    SECTION( "Resize to larger dimensions" )
    {
        Bitmap bmp( 4, 4 );
        FillSolid( bmp, 0xAB, 0xCD, 0xEF );
        bmp.Resize( 20, 20 );
        REQUIRE( bmp.Width() == 20 );
        REQUIRE( bmp.Height() == 20 );
        VerifyAll( bmp, MakePixel( 0xAB, 0xCD, 0xEF ) );
    }

    SECTION( "ResizeNew leaves original unchanged" )
    {
        Bitmap bmp( 8, 8 );
        FillPattern( bmp );
        auto snapshot = Snapshot( bmp );

        auto resized = bmp.ResizeNew( 3, 3 );
        REQUIRE( resized != nullptr );
        REQUIRE( resized->Width() == 3 );
        REQUIRE( resized->Height() == 3 );
        REQUIRE( resized->Orientation() == bmp.Orientation() );

        // Original is untouched
        REQUIRE( bmp.Width() == 8 );
        REQUIRE( bmp.Height() == 8 );
        REQUIRE( Snapshot( bmp ) == snapshot );
    }

    SECTION( "Resize with TaskDispatch" )
    {
        Bitmap bmp( 16, 16 );
        FillSolid( bmp, 0x01, 0x02, 0x03 );

        TaskDispatch td( 4, "test-resize" );
        bmp.Resize( 9, 11, &td );

        REQUIRE( bmp.Width() == 9 );
        REQUIRE( bmp.Height() == 11 );
        VerifyAll( bmp, MakePixel( 0x01, 0x02, 0x03 ) );
    }

    SECTION( "ResizeNew with TaskDispatch" )
    {
        Bitmap bmp( 12, 12 );
        FillSolid( bmp, 0x04, 0x05, 0x06 );

        TaskDispatch td( 2, "test-resizenew" );
        auto resized = bmp.ResizeNew( 5, 5, &td );

        REQUIRE( resized != nullptr );
        REQUIRE( resized->Width() == 5 );
        REQUIRE( resized->Height() == 5 );
        VerifyAll( *resized, MakePixel( 0x04, 0x05, 0x06 ) );
    }
}

TEST_CASE( "Bitmap extend", "[bitmap][extend]" )
{
    SECTION( "Extend preserves original and zeroes new area" )
    {
        Bitmap bmp( 2, 2 );
        FillPattern( bmp );

        bmp.Extend( 4, 4 );
        REQUIRE( bmp.Width() == 4 );
        REQUIRE( bmp.Height() == 4 );

        // Original pixels preserved in the top-left corner
        for( uint32_t y = 0; y < 2; y++ )
        {
            for( uint32_t x = 0; x < 2; x++ )
            {
                const auto r = uint8_t( ( x * 7 + y * 3 ) & 0xFF );
                const auto g = uint8_t( ( x * 5 + y * 11 ) & 0xFF );
                const auto b = uint8_t( ( x * 13 + y * 17 ) & 0xFF );
                REQUIRE( GetPixel( bmp, x, y ) == MakePixel( r, g, b ) );
            }
        }

        // Extended area is zeroed (RGBA = 0x00000000)
        for( uint32_t y = 0; y < 4; y++ )
        {
            for( uint32_t x = 0; x < 4; x++ )
            {
                if( x < 2 && y < 2 ) continue;
                REQUIRE( GetPixel( bmp, x, y ) == 0x00000000 );
            }
        }
    }

    SECTION( "Extend only height" )
    {
        Bitmap bmp( 2, 2 );
        FillSolid( bmp, 0xAA, 0xBB, 0xCC );
        bmp.Extend( 2, 5 );
        REQUIRE( bmp.Width() == 2 );
        REQUIRE( bmp.Height() == 5 );

        // Original rows preserved, new rows zeroed
        for( uint32_t y = 0; y < 2; y++ )
        {
            for( uint32_t x = 0; x < 2; x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == MakePixel( 0xAA, 0xBB, 0xCC ) );
            }
        }
        for( uint32_t y = 2; y < 5; y++ )
        {
            for( uint32_t x = 0; x < 2; x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == 0x00000000 );
            }
        }
    }
}

TEST_CASE( "Bitmap crop", "[bitmap][crop]" )
{
    SECTION( "Crop preserves selected region" )
    {
        Bitmap bmp( 4, 4 );
        FillPattern( bmp );

        bmp.Crop( 1, 1, 2, 2 );
        REQUIRE( bmp.Width() == 2 );
        REQUIRE( bmp.Height() == 2 );

        for( uint32_t y = 0; y < 2; y++ )
        {
            for( uint32_t x = 0; x < 2; x++ )
            {
                const auto r = uint8_t( ( ( x + 1 ) * 7 + ( y + 1 ) * 3 ) & 0xFF );
                const auto g = uint8_t( ( ( x + 1 ) * 5 + ( y + 1 ) * 11 ) & 0xFF );
                const auto b = uint8_t( ( ( x + 1 ) * 13 + ( y + 1 ) * 17 ) & 0xFF );
                REQUIRE( GetPixel( bmp, x, y ) == MakePixel( r, g, b ) );
            }
        }
    }

    SECTION( "Crop full image" )
    {
        Bitmap bmp( 3, 3 );
        FillPattern( bmp );
        auto snapshot = Snapshot( bmp );
        bmp.Crop( 0, 0, 3, 3 );
        REQUIRE( Snapshot( bmp ) == snapshot );
    }
}

TEST_CASE( "Bitmap fill black", "[bitmap][fill]" )
{
    SECTION( "Fill region leaves alpha at 0xFF" )
    {
        Bitmap bmp( 4, 4 );
        FillSolid( bmp, 0xFF, 0xFF, 0xFF );

        bmp.FillBlack( 1, 1, 2, 2 );
        REQUIRE( GetPixel( bmp, 0, 0 ) == MakePixel( 0xFF, 0xFF, 0xFF ) );
        REQUIRE( GetPixel( bmp, 1, 1 ) == 0xFF000000 );
        REQUIRE( GetPixel( bmp, 2, 2 ) == 0xFF000000 );
        REQUIRE( GetPixel( bmp, 3, 3 ) == MakePixel( 0xFF, 0xFF, 0xFF ) );
    }

    SECTION( "Fill entire image" )
    {
        Bitmap bmp( 3, 3 );
        FillSolid( bmp, 0x11, 0x22, 0x33 );
        bmp.FillBlack( 0, 0, 3, 3 );
        VerifyAll( bmp, 0xFF000000 );
    }
}

TEST_CASE( "Bitmap set alpha", "[bitmap][alpha]" )
{
    SECTION( "Set alpha to 0xFF preserves RGB" )
    {
        Bitmap bmp( 8, 8 ); // 64 pixels exercises SIMD paths
        FillSolid( bmp, 0x12, 0x34, 0x56, 0x00 );

        bmp.SetAlpha( 0xFF );
        VerifyAll( bmp, MakePixel( 0x12, 0x34, 0x56, 0xFF ) );
    }

    SECTION( "Set alpha to arbitrary value" )
    {
        Bitmap bmp( 8, 8 );
        FillSolid( bmp, 0x12, 0x34, 0x56, 0xFF );

        bmp.SetAlpha( 0x80 );
        VerifyAll( bmp, MakePixel( 0x12, 0x34, 0x56, 0x80 ) );
    }

    SECTION( "Set alpha to zero" )
    {
        Bitmap bmp( 8, 8 );
        FillSolid( bmp, 0xAB, 0xCD, 0xEF, 0xFF );

        bmp.SetAlpha( 0x00 );
        VerifyAll( bmp, MakePixel( 0xAB, 0xCD, 0xEF, 0x00 ) );
    }

    SECTION( "Small bitmap exercises scalar tail" )
    {
        Bitmap bmp( 1, 2 );
        FillSolid( bmp, 0x01, 0x02, 0x03, 0xFF );
        bmp.SetAlpha( 0x40 );
        VerifyAll( bmp, MakePixel( 0x01, 0x02, 0x03, 0x40 ) );
    }
}

TEST_CASE( "Bitmap flips and rotations", "[bitmap][transform]" )
{
    SECTION( "FlipVertical" )
    {
        Bitmap bmp( 3, 3 );
        FillPattern( bmp );
        const auto orig = Snapshot( bmp );
        const auto w = bmp.Width();

        bmp.FlipVertical();
        for( uint32_t y = 0; y < 3; y++ )
        {
            for( uint32_t x = 0; x < 3; x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == orig[size_t( 2 - y ) * w + x] );
            }
        }
    }

    SECTION( "FlipHorizontal" )
    {
        Bitmap bmp( 3, 3 );
        FillPattern( bmp );
        const auto orig = Snapshot( bmp );
        const auto w = bmp.Width();

        bmp.FlipHorizontal();
        for( uint32_t y = 0; y < 3; y++ )
        {
            for( uint32_t x = 0; x < 3; x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == orig[size_t( y ) * w + ( 2 - x )] );
            }
        }
    }

    SECTION( "Rotate90" )
    {
        Bitmap bmp( 3, 2 );
        FillPattern( bmp );
        const auto orig = Snapshot( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();

        bmp.Rotate90();
        REQUIRE( bmp.Width() == h );
        REQUIRE( bmp.Height() == w );
        for( uint32_t y = 0; y < bmp.Height(); y++ )
        {
            for( uint32_t x = 0; x < bmp.Width(); x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == orig[size_t( h - 1 - x ) * w + y] );
            }
        }
    }

    SECTION( "Rotate180" )
    {
        Bitmap bmp( 3, 2 );
        FillPattern( bmp );
        const auto orig = Snapshot( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();

        bmp.Rotate180();
        REQUIRE( bmp.Width() == w );
        REQUIRE( bmp.Height() == h );
        for( uint32_t y = 0; y < h; y++ )
        {
            for( uint32_t x = 0; x < w; x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == orig[size_t( h - 1 - y ) * w + ( w - 1 - x )] );
            }
        }
    }

    SECTION( "Rotate270" )
    {
        Bitmap bmp( 3, 2 );
        FillPattern( bmp );
        const auto orig = Snapshot( bmp );
        const auto w = bmp.Width();
        const auto h = bmp.Height();

        bmp.Rotate270();
        REQUIRE( bmp.Width() == h );
        REQUIRE( bmp.Height() == w );
        for( uint32_t y = 0; y < bmp.Height(); y++ )
        {
            for( uint32_t x = 0; x < bmp.Width(); x++ )
            {
                REQUIRE( GetPixel( bmp, x, y ) == orig[size_t( x ) * w + ( w - 1 - y )] );
            }
        }
    }

    SECTION( "Four rotations return to original" )
    {
        Bitmap bmp( 4, 2 );
        FillPattern( bmp );
        auto snapshot = Snapshot( bmp );

        bmp.Rotate90();
        bmp.Rotate90();
        bmp.Rotate90();
        bmp.Rotate90();
        REQUIRE( Snapshot( bmp ) == snapshot );
    }
}

TEST_CASE( "Bitmap normalize orientation", "[bitmap][orientation]" )
{
    SECTION( "Orientations 0 and 1 are no-ops" )
    {
        for( int orientation : { 0, 1 } )
        {
            Bitmap bmp( 3, 3, orientation );
            FillPattern( bmp );
            auto snapshot = Snapshot( bmp );
            bmp.NormalizeOrientation();
            REQUIRE( bmp.Orientation() == orientation );
            REQUIRE( Snapshot( bmp ) == snapshot );
        }
    }

    SECTION( "Each orientation applies the documented transform sequence" )
    {
        struct Case
        {
            int orientation;
            std::vector<std::function<void(Bitmap&)>> ops;
        };

        const std::vector<Case> cases = {
            { 2, { []( Bitmap& b ) { b.FlipHorizontal(); } } },
            { 3, { []( Bitmap& b ) { b.Rotate180(); } } },
            { 4, { []( Bitmap& b ) { b.FlipVertical(); } } },
            { 5, { []( Bitmap& b ) { b.Rotate270(); b.FlipVertical(); } } },
            { 6, { []( Bitmap& b ) { b.Rotate90(); } } },
            { 7, { []( Bitmap& b ) { b.Rotate90(); b.FlipVertical(); } } },
            { 8, { []( Bitmap& b ) { b.Rotate270(); } } },
        };

        for( const auto& testCase : cases )
        {
            Bitmap normalized( 4, 3, testCase.orientation );
            Bitmap manual( 4, 3, testCase.orientation );
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
                    REQUIRE( GetPixel( normalized, x, y ) == GetPixel( manual, x, y ) );
                }
            }
        }
    }

    SECTION( "Invalid orientation aborts" )
    {
        Bitmap bmp( 2, 2, 9 );
        REQUIRE_ABORTS( bmp.NormalizeOrientation() );
    }
}

TEST_CASE( "Bitmap bgr to rgb", "[bitmap][colorspace]" )
{
    SECTION( "Swaps red and blue channels" )
    {
        Bitmap bmp( 3, 3 );
        FillPattern( bmp );

        bmp.BgrToRgb();
        for( uint32_t y = 0; y < 3; y++ )
        {
            for( uint32_t x = 0; x < 3; x++ )
            {
                const auto r = uint8_t( ( x * 7 + y * 3 ) & 0xFF );
                const auto g = uint8_t( ( x * 5 + y * 11 ) & 0xFF );
                const auto b = uint8_t( ( x * 13 + y * 17 ) & 0xFF );
                REQUIRE( GetPixel( bmp, x, y ) == MakePixel( b, g, r ) );
            }
        }
    }

    SECTION( "Single pixel" )
    {
        Bitmap bmp( 1, 1 );
        SetPixel( bmp, 0, 0, MakePixel( 0x11, 0x22, 0x33 ) );
        bmp.BgrToRgb();
        REQUIRE( GetPixel( bmp, 0, 0 ) == MakePixel( 0x33, 0x22, 0x11 ) );
    }

    SECTION( "Alpha channel is untouched" )
    {
        Bitmap bmp( 2, 2 );
        SetPixel( bmp, 0, 0, MakePixel( 0x11, 0x22, 0x33, 0x80 ) );
        bmp.BgrToRgb();
        REQUIRE( GetPixel( bmp, 0, 0 ) == MakePixel( 0x33, 0x22, 0x11, 0x80 ) );
    }
}

TEST_CASE( "Bitmap save png", "[bitmap][png]" )
{
    SECTION( "SavePng to path writes a valid PNG file" )
    {
        Bitmap bmp( 4, 4 );
        FillSolid( bmp, 0x12, 0x34, 0x56 );
        auto tempFile = TempFile::createEmpty();

        REQUIRE( bmp.SavePng( tempFile.path() ) == true );
        REQUIRE( tempFile.size() > 0 );

        // Verify PNG signature
        auto data = BinaryPattern::random( 8 );
        FILE* f = fopen( tempFile.path(), "rb" );
        REQUIRE( f != nullptr );
        REQUIRE( fread( data.data(), 1, 8, f ) == 8 );
        fclose( f );

        const char signature[8] = { '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n' };
        REQUIRE( memcmp( data.data(), signature, 8 ) == 0 );
    }

    SECTION( "SavePng to fd writes a valid PNG file" )
    {
        Bitmap bmp( 3, 3 );
        FillSolid( bmp, 0xAA, 0xBB, 0xCC );
        auto tempFile = TempFile::createEmpty();

        FILE* f = fopen( tempFile.path(), "wb" );
        REQUIRE( f != nullptr );
        REQUIRE( bmp.SavePng( fileno( f ) ) == true );
        fclose( f );

        REQUIRE( tempFile.size() > 0 );
        auto data = BinaryPattern::random( 8 );
        FILE* r = fopen( tempFile.path(), "rb" );
        REQUIRE( r != nullptr );
        REQUIRE( fread( data.data(), 1, 8, r ) == 8 );
        fclose( r );

        const char signature[8] = { '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n' };
        REQUIRE( memcmp( data.data(), signature, 8 ) == 0 );
    }

    SECTION( "SavePng to unwritable path returns false" )
    {
        Bitmap bmp( 2, 2 );
        REQUIRE( bmp.SavePng( "/nonexistent_dir_mcore/file.png" ) == false );
    }

    SECTION( "SavePng to invalid fd returns false" )
    {
        Bitmap bmp( 2, 2 );
        REQUIRE( bmp.SavePng( -1 ) == false );
    }
}

TEST_CASE( "Bitmap panic paths abort", "[bitmap][panic]" )
{
    SECTION( "Extend with smaller dimensions" )
    {
        Bitmap bmp( 4, 4 );
        REQUIRE_ABORTS( bmp.Extend( 2, 2 ) );
    }

    SECTION( "Extend with smaller height" )
    {
        Bitmap bmp( 4, 4 );
        REQUIRE_ABORTS( bmp.Extend( 4, 3 ) );
    }

    SECTION( "Crop out of bounds" )
    {
        Bitmap bmp( 3, 3 );
        REQUIRE_ABORTS( bmp.Crop( 2, 2, 2, 2 ) );
    }

    SECTION( "FillBlack out of bounds" )
    {
        Bitmap bmp( 3, 3 );
        REQUIRE_ABORTS( bmp.FillBlack( 1, 1, 3, 3 ) );
    }
}
