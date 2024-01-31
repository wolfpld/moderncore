#include <gtest/gtest.h>

#include "util/Panic.hpp"

TEST(PanicTest, CheckPanic) {
    // Call CheckPanic with a true condition
    CheckPanic(true, "This should not panic");

    // Call CheckPanic with a false condition
    EXPECT_DEATH(CheckPanic(false, "This should panic"), "");
}
