#include <gtest/gtest.h>

#include "util/Bitmap.hpp"

// Test case for flipping a bitmap vertically
TEST(BitmapTest, FlipVertical) {
    // Create a test bitmap
    Bitmap bitmap(2, 2);
    {
        auto ptr = (uint32_t*)bitmap.Data();
        ptr[0] = 1;
        ptr[1] = 2;
        ptr[2] = 3;
        ptr[3] = 4;
    }

    // Flip the bitmap vertically
    bitmap.FlipVertical();

    // Check that the bitmap was flipped correctly
    auto data = (uint32_t*)bitmap.Data();
    EXPECT_EQ(data[0], 3);
    EXPECT_EQ(data[1], 4);
    EXPECT_EQ(data[2], 1);
    EXPECT_EQ(data[3], 2);
}

// Test case for getting the width and height of a bitmap
TEST(BitmapTest, WidthAndHeight) {
    // Create a test bitmap
    Bitmap bitmap(3, 4);

    // Check that the bitmap has the correct width and height
    EXPECT_EQ(bitmap.Width(), 3);
    EXPECT_EQ(bitmap.Height(), 4);
}
