#include "TestUtils.hpp"
#include <catch2/catch_all.hpp>
#include <src/util/Bitmap.hpp>
#include <src/util/VectorImage.hpp>
#include <memory>

namespace
{

class MockVectorImage : public VectorImage
{
public:
    MockVectorImage() = default;

    [[nodiscard]] bool IsValid() const override { return m_valid; }
    [[nodiscard]] std::unique_ptr<Bitmap> Rasterize( int width, int height ) const override
    {
        return std::make_unique<Bitmap>( width, height );
    }

    void SetValid( bool valid ) { m_valid = valid; }

private:
    bool m_valid = true;
};

}

TEST_CASE( "VectorImage base class", "[vectorimage]" )
{
    SECTION( "Default width and height are -1" )
    {
        MockVectorImage image;
        REQUIRE( image.Width() == -1 );
        REQUIRE( image.Height() == -1 );
    }

    SECTION( "IsValid is polymorphic" )
    {
        MockVectorImage image;
        REQUIRE( image.IsValid() == true );
        image.SetValid( false );
        REQUIRE( image.IsValid() == false );
    }

    SECTION( "Rasterize produces a bitmap of requested size" )
    {
        MockVectorImage image;
        auto bmp = image.Rasterize( 4, 3 );
        REQUIRE( bmp != nullptr );
        REQUIRE( bmp->Width() == 4 );
        REQUIRE( bmp->Height() == 3 );
    }

    SECTION( "Usable through a base class pointer" )
    {
        std::unique_ptr<VectorImage> image = std::make_unique<MockVectorImage>();
        REQUIRE( image->IsValid() == true );
        REQUIRE( image->Width() == -1 );
        auto bmp = image->Rasterize( 2, 2 );
        REQUIRE( bmp != nullptr );
    }

    SECTION( "Is non-copyable" )
    {
        static_assert( !std::is_copy_constructible<VectorImage>::value );
        static_assert( !std::is_copy_assignable<VectorImage>::value );
    }
}
