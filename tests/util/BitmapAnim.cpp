#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Bitmap.hpp>
#include <src/util/BitmapAnim.hpp>
#include <src/util/TaskDispatch.hpp>
#include <memory>

TEST_CASE( "BitmapAnim frame management", "[bitmapanim][frames]" )
{
    SECTION( "Starts with no frames" )
    {
        BitmapAnim anim( 0 );
        REQUIRE( anim.FrameCount() == 0 );
    }

    SECTION( "AddFrame appends frames" )
    {
        BitmapAnim anim( 4 );

        auto bmp1 = std::make_shared<Bitmap>( 2, 2 );
        auto bmp2 = std::make_shared<Bitmap>( 3, 3 );

        anim.AddFrame( bmp1, 100 );
        anim.AddFrame( bmp2, 200 );

        REQUIRE( anim.FrameCount() == 2 );
        REQUIRE( anim.GetFrame( 0 ).bmp.get() == bmp1.get() );
        REQUIRE( anim.GetFrame( 0 ).delay_us == 100 );
        REQUIRE( anim.GetFrame( 1 ).bmp.get() == bmp2.get() );
        REQUIRE( anim.GetFrame( 1 ).delay_us == 200 );
    }

    SECTION( "Const accessor returns the same frames" )
    {
        BitmapAnim anim( 2 );
        auto bmp1 = std::make_shared<Bitmap>( 4, 4 );
        anim.AddFrame( bmp1, 50 );

        const BitmapAnim& ref = anim;
        REQUIRE( ref.GetFrame( 0 ).bmp.get() == bmp1.get() );
        REQUIRE( ref.GetFrame( 0 ).delay_us == 50 );
    }

    SECTION( "Frame count respects constructor reservation" )
    {
        BitmapAnim anim( 8 );
        for( uint32_t i = 0; i < 8; i++ )
        {
            anim.AddFrame( std::make_shared<Bitmap>( 1, 1 ), i * 10 );
        }
        REQUIRE( anim.FrameCount() == 8 );
        REQUIRE( anim.GetFrame( 7 ).delay_us == 70 );
    }
}

TEST_CASE( "BitmapAnim resize", "[bitmapanim][resize]" )
{
    SECTION( "Resize updates all frames" )
    {
        BitmapAnim anim( 2 );
        anim.AddFrame( std::make_shared<Bitmap>( 8, 8 ), 100 );
        anim.AddFrame( std::make_shared<Bitmap>( 10, 12 ), 200 );

        anim.Resize( 4, 5 );

        REQUIRE( anim.GetFrame( 0 ).bmp->Width() == 4 );
        REQUIRE( anim.GetFrame( 0 ).bmp->Height() == 5 );
        REQUIRE( anim.GetFrame( 1 ).bmp->Width() == 4 );
        REQUIRE( anim.GetFrame( 1 ).bmp->Height() == 5 );
    }

    SECTION( "Resize with TaskDispatch" )
    {
        BitmapAnim anim( 2 );
        anim.AddFrame( std::make_shared<Bitmap>( 6, 6 ), 100 );
        anim.AddFrame( std::make_shared<Bitmap>( 6, 6 ), 100 );

        TaskDispatch td( 2, "anim-resize" );
        anim.Resize( 3, 3, &td );

        REQUIRE( anim.GetFrame( 0 ).bmp->Width() == 3 );
        REQUIRE( anim.GetFrame( 0 ).bmp->Height() == 3 );
        REQUIRE( anim.GetFrame( 1 ).bmp->Width() == 3 );
        REQUIRE( anim.GetFrame( 1 ).bmp->Height() == 3 );
    }

    SECTION( "Resize empty animation is a no-op" )
    {
        BitmapAnim anim( 0 );
        anim.Resize( 4, 4 );
        REQUIRE( anim.FrameCount() == 0 );
    }
}

TEST_CASE( "BitmapAnim normalize size", "[bitmapanim][normalize]" )
{
    SECTION( "Uniform sizes are unchanged" )
    {
        BitmapAnim anim( 2 );
        anim.AddFrame( std::make_shared<Bitmap>( 4, 4 ), 100 );
        anim.AddFrame( std::make_shared<Bitmap>( 4, 4 ), 100 );

        anim.NormalizeSize();

        REQUIRE( anim.GetFrame( 0 ).bmp->Width() == 4 );
        REQUIRE( anim.GetFrame( 0 ).bmp->Height() == 4 );
        REQUIRE( anim.GetFrame( 1 ).bmp->Width() == 4 );
        REQUIRE( anim.GetFrame( 1 ).bmp->Height() == 4 );
    }

    SECTION( "Empty animation is a no-op" )
    {
        BitmapAnim anim( 0 );
        anim.NormalizeSize();
        REQUIRE( anim.FrameCount() == 0 );
    }

    SECTION( "Mixed sizes extend smaller frames to the maximum" )
    {
        BitmapAnim anim( 3 );
        anim.AddFrame( std::make_shared<Bitmap>( 2, 2 ), 100 );
        anim.AddFrame( std::make_shared<Bitmap>( 5, 3 ), 100 );
        anim.AddFrame( std::make_shared<Bitmap>( 4, 6 ), 100 );

        anim.NormalizeSize();

        // All frames become the max dimensions (5x6)
        for( size_t i = 0; i < anim.FrameCount(); i++ )
        {
            REQUIRE( anim.GetFrame( i ).bmp->Width() == 5 );
            REQUIRE( anim.GetFrame( i ).bmp->Height() == 6 );
        }
    }

    SECTION( "Extending preserves the original frame content" )
    {
        BitmapAnim anim( 2 );
        auto small = std::make_shared<Bitmap>( 2, 2 );
        auto large = std::make_shared<Bitmap>( 3, 3 );

        // Mark the small frame's top-left pixel
        uint32_t* p = (uint32_t*)small->Data();
        *p = 0xff102030;

        anim.AddFrame( small, 100 );
        anim.AddFrame( large, 100 );

        anim.NormalizeSize();

        // Small frame was extended to 3x3, original pixel preserved
        REQUIRE( anim.GetFrame( 0 ).bmp->Width() == 3 );
        REQUIRE( anim.GetFrame( 0 ).bmp->Height() == 3 );
        REQUIRE( *(uint32_t*)anim.GetFrame( 0 ).bmp->Data() == 0xff102030 );
    }
}

TEST_CASE( "BitmapAnim is non-copyable", "[bitmapanim][nocopy]" )
{
    static_assert( !std::is_copy_constructible<BitmapAnim>::value );
    static_assert( !std::is_copy_assignable<BitmapAnim>::value );
}
