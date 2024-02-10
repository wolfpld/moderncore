#include <gtest/gtest.h>

#include "util/FileBuffer.hpp"

TEST(FileBufferTest, LoadFile) {
    // Create a test file with some data
    const char* filename = "test.bin";
    FILE* file = fopen(filename, "wb");
    fwrite("test data", 1, 10, file);
    fclose(file);

    // Load the test file into a file buffer
    FileBuffer buffer(filename);

    // Check that the buffer contains the correct data
    EXPECT_EQ(buffer.size(), 10);
    EXPECT_STREQ(buffer.data(), "test data");

    // Delete the test file
    std::remove(filename);
}
