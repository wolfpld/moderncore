#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <contrib/half.hpp>
#include <src/util/BitmapHdr.hpp>
#include <src/util/BitmapHdrHalf.hpp>
#include <src/util/TaskDispatch.hpp>
#include <string.h>
#include <vector>

namespace
{

void SetPixel( BitmapHdrHalf& bmp, uint32_t x, uint32_t y, float r, float g, float b, float a )
{
    half_float::half* p = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

void FillSolid( BitmapHdrHalf& bmp, float r, float g, float b, float a = 1.0f )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            SetPixel( bmp, x, y, r, g, b, a );
        }
    }
}

void VerifySolid( const BitmapHdrHalf& bmp, float r, float g, float b, float a )
{
    for( uint32_t y = 0; y < bmp.Height(); y++ )
    {
        for( uint32_t x = 0; x < bmp.Width(); x++ )
        {
            const half_float::half* p = bmp.Data() + ( size_t( y ) * bmp.Width() + x ) * 4;
            REQUIRE( float( p[0] ) == Catch::Approx( r ).margin( 0.01f ) );
            REQUIRE( float( p[1] ) == Catch::Approx( g ).margin( 0.01f ) );
            REQUIRE( float( p[2] ) == Catch::Approx( b ).margin( 0.01f ) );
            REQUIRE( float( p[3] ) == Catch::Approx( a ).margin( 0.01f ) );
        }
    }
}

}

TEST_CASE( "BitmapHdrHalf constructor and accessors", "[bitmaphdrhalf]" )
{
    SECTION( "Direct construction" )
    {
        BitmapHdrHalf bmp( 4, 2, Colorspace::BT2020, 5 );
        REQUIRE( bmp.Width() == 4 );
        REQUIRE( bmp.Height() == 2 );
        REQUIRE( bmp.Data() != nullptr );
        REQUIRE( bmp.Orientation() == 5 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT2020 );
    }

    SECTION( "Conversion from BitmapHdr" )
    {
        BitmapHdr hdr( 3, 2, Colorspace::BT709, 4 );
        float* src = hdr.Data();
        for( uint32_t i = 0; i < 3 * 2 * 4; i++ )
        {
            src[i] = ( i % 4 == 3 ) ? 1.0f : ( i + 1 ) * 0.25f;
        }

        BitmapHdrHalf bmp( hdr );
        REQUIRE( bmp.Width() == 3 );
        REQUIRE( bmp.Height() == 2 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );
        REQUIRE( bmp.Orientation() == 4 );

        for( uint32_t i = 0; i < 3 * 2 * 4; i++ )
        {
            if( i % 4 == 3 )
            {
                REQUIRE( float( bmp.Data()[i] ) == Catch::Approx( 1.0f ).margin( 0.01f ) );
            }
            else
            {
                REQUIRE( float( bmp.Data()[i] ) == Catch::Approx( ( i + 1 ) * 0.25f ).margin( 0.01f ) );
            }
        }
    }
}

TEST_CASE( "BitmapHdrHalf resize", "[bitmaphdrhalf][resize]" )
{
    SECTION( "Resize preserves solid color" )
    {
        BitmapHdrHalf bmp( 16, 16, Colorspace::BT709 );
        FillSolid( bmp, 0.25f, 0.5f, 0.75f );

        bmp.Resize( 7, 5 );
        REQUIRE( bmp.Width() == 7 );
        REQUIRE( bmp.Height() == 5 );
        VerifySolid( bmp, 0.25f, 0.5f, 0.75f, 1.0f );
    }

    SECTION( "ResizeNew leaves original unchanged" )
    {
        BitmapHdrHalf bmp( 8, 8, Colorspace::BT2020, 3 );
        FillSolid( bmp, 0.1f, 0.2f, 0.3f );

        auto resized = bmp.ResizeNew( 3, 3 );
        REQUIRE( resized != nullptr );
        REQUIRE( resized->Width() == 3 );
        REQUIRE( resized->Height() == 3 );
        REQUIRE( resized->GetColorspace() == Colorspace::BT2020 );
        REQUIRE( resized->Orientation() == 3 );

        REQUIRE( bmp.Width() == 8 );
        REQUIRE( bmp.Height() == 8 );
    }

    SECTION( "Resize with TaskDispatch" )
    {
        BitmapHdrHalf bmp( 12, 12, Colorspace::BT709 );
        FillSolid( bmp, 0.3f, 0.4f, 0.5f );

        TaskDispatch td( 2, "half-resize" );
        bmp.Resize( 5, 5, &td );

        REQUIRE( bmp.Width() == 5 );
        REQUIRE( bmp.Height() == 5 );
        VerifySolid( bmp, 0.3f, 0.4f, 0.5f, 1.0f );
    }
}

TEST_CASE( "BitmapHdrHalf crop and fill", "[bitmaphdrhalf][crop][fill]" )
{
    SECTION( "Crop preserves selected region" )
    {
        BitmapHdrHalf bmp( 4, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.2f, 0.4f, 0.6f );

        bmp.Crop( 1, 1, 2, 2 );
        REQUIRE( bmp.Width() == 2 );
        REQUIRE( bmp.Height() == 2 );
        VerifySolid( bmp, 0.2f, 0.4f, 0.6f, 1.0f );
    }

    SECTION( "FillBlack zeroes RGB and sets alpha to one" )
    {
        BitmapHdrHalf bmp( 4, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.5f, 0.5f, 0.5f, 0.5f );

        bmp.FillBlack( 1, 1, 2, 2 );

        for( uint32_t y = 0; y < 4; y++ )
        {
            for( uint32_t x = 0; x < 4; x++ )
            {
                const half_float::half* p = bmp.Data() + ( size_t( y ) * 4 + x ) * 4;
                if( x >= 1 && x <= 2 && y >= 1 && y <= 2 )
                {
                    REQUIRE( float( p[0] ) == Catch::Approx( 0.0f ) );
                    REQUIRE( float( p[1] ) == Catch::Approx( 0.0f ) );
                    REQUIRE( float( p[2] ) == Catch::Approx( 0.0f ) );
                    REQUIRE( float( p[3] ) == Catch::Approx( 1.0f ) );
                }
                else
                {
                    REQUIRE( float( p[0] ) == Catch::Approx( 0.5f ) );
                    REQUIRE( float( p[3] ) == Catch::Approx( 0.5f ) );
                }
            }
        }
    }

    SECTION( "FillBlack entire image" )
    {
        BitmapHdrHalf bmp( 3, 3, Colorspace::BT709 );
        FillSolid( bmp, 0.3f, 0.3f, 0.3f, 0.3f );

        bmp.FillBlack( 0, 0, 3, 3 );
        VerifySolid( bmp, 0.0f, 0.0f, 0.0f, 1.0f );
    }
}

TEST_CASE( "BitmapHdrHalf colorspace transforms", "[bitmaphdrhalf][colorspace]" )
{
    SECTION( "BT2020 to BT709 transforms pixels and preserves alpha" )
    {
        BitmapHdrHalf bmp( 8, 8, Colorspace::BT2020 );
        FillSolid( bmp, 0.2f, 0.5f, 0.8f, 0.75f );

        bmp.SetColorspace( Colorspace::BT709 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );

        const half_float::half* p = bmp.Data();
        REQUIRE( float( p[3] ) == Catch::Approx( 0.75f ).margin( 0.01f ) );
        REQUIRE( float( p[0] ) != Catch::Approx( 0.2f ) );
    }

    SECTION( "BT709 to BT2020 transforms pixels" )
    {
        BitmapHdrHalf bmp( 8, 8, Colorspace::BT709 );
        FillSolid( bmp, 0.2f, 0.5f, 0.8f, 1.0f );

        bmp.SetColorspace( Colorspace::BT2020 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT2020 );

        const half_float::half* p = bmp.Data();
        REQUIRE( float( p[3] ) == Catch::Approx( 1.0f ).margin( 0.01f ) );
        REQUIRE( float( p[0] ) != Catch::Approx( 0.2f ) );
    }

    SECTION( "No-op transform leaves data unchanged" )
    {
        BitmapHdrHalf bmp( 4, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.3f, 0.4f, 0.5f );

        bmp.SetColorspace( Colorspace::BT709 );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );
        VerifySolid( bmp, 0.3f, 0.4f, 0.5f, 1.0f );
    }

    SECTION( "Transform with TaskDispatch" )
    {
        BitmapHdrHalf bmp( 16, 16, Colorspace::BT2020 );
        FillSolid( bmp, 0.1f, 0.3f, 0.6f, 0.5f );

        TaskDispatch td( 4, "half-colorspace" );
        bmp.SetColorspace( Colorspace::BT709, &td );
        REQUIRE( bmp.GetColorspace() == Colorspace::BT709 );

        const half_float::half* p = bmp.Data();
        REQUIRE( float( p[3] ) == Catch::Approx( 0.5f ).margin( 0.01f ) );
        REQUIRE( float( p[0] ) != Catch::Approx( 0.1f ) );
    }
}

TEST_CASE( "BitmapHdrHalf save exr", "[bitmaphdrhalf][exr]" )
{
    SECTION( "SaveExr to path writes a valid EXR file" )
    {
        BitmapHdrHalf bmp( 4, 4, Colorspace::BT709 );
        FillSolid( bmp, 0.25f, 0.5f, 0.75f );
        auto tempFile = TempFile::createEmpty();

        REQUIRE( bmp.SaveExr( tempFile.path() ) == true );
        REQUIRE( tempFile.size() > 0 );

        // Verify EXR magic number (0x01312F76 little-endian)
        char magic[4] = { 0, 0, 0, 0 };
        FILE* f = fopen( tempFile.path(), "rb" );
        REQUIRE( f != nullptr );
        REQUIRE( fread( magic, 1, 4, f ) == 4 );
        fclose( f );

        REQUIRE( magic[0] == '\x76' );
        REQUIRE( magic[1] == '\x2f' );
        REQUIRE( magic[2] == '\x31' );
        REQUIRE( magic[3] == '\x01' );
    }

    SECTION( "SaveExr to fd writes a valid EXR file" )
    {
        BitmapHdrHalf bmp( 3, 3, Colorspace::BT2020 );
        FillSolid( bmp, 0.2f, 0.4f, 0.6f );
        auto tempFile = TempFile::createEmpty();

        FILE* f = fopen( tempFile.path(), "wb" );
        REQUIRE( f != nullptr );
        REQUIRE( bmp.SaveExr( fileno( f ) ) == true );
        fclose( f );

        REQUIRE( tempFile.size() > 0 );
        char magic[4] = { 0, 0, 0, 0 };
        FILE* r = fopen( tempFile.path(), "rb" );
        REQUIRE( r != nullptr );
        REQUIRE( fread( magic, 1, 4, r ) == 4 );
        fclose( r );

        REQUIRE( magic[0] == '\x76' );
        REQUIRE( magic[1] == '\x2f' );
        REQUIRE( magic[2] == '\x31' );
        REQUIRE( magic[3] == '\x01' );
    }

    SECTION( "SaveExr to unwritable path returns false" )
    {
        BitmapHdrHalf bmp( 2, 2, Colorspace::BT709 );
        REQUIRE( bmp.SaveExr( "/nonexistent_dir_mcore/file.exr" ) == false );
    }
}

TEST_CASE( "BitmapHdrHalf panic paths abort", "[bitmaphdrhalf][panic]" )
{
    SECTION( "Crop out of bounds" )
    {
        BitmapHdrHalf bmp( 3, 3, Colorspace::BT709 );
        REQUIRE_ABORTS( bmp.Crop( 2, 2, 2, 2 ) );
    }

    SECTION( "FillBlack out of bounds" )
    {
        BitmapHdrHalf bmp( 3, 3, Colorspace::BT709 );
        REQUIRE_ABORTS( bmp.FillBlack( 1, 1, 3, 3 ) );
    }
}
