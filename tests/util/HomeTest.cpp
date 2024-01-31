#include <gtest/gtest.h>

#include "util/Home.hpp"

TEST(HomeTest, GetHome) {
    // Check that GetHome returns the correct home directory
    EXPECT_EQ(GetHome(), getenv("HOME"));
}

TEST(HomeTest, ExpandHome) {
    // Check that ExpandHome returns the correct expanded path
    EXPECT_EQ(ExpandHome("~/test"), std::string(getenv("HOME")) + "/test");

    // Check that ExpandHome returns the original path if it doesn't start with ~
    EXPECT_EQ(ExpandHome("/test"), "/test");
}
