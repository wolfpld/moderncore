#include <gtest/gtest.h>

#include "util/FileWrapper.hpp"

TEST(FileWrapperTest, ReadFile) {
    // Create a test file with some data
    const char* filename = "test.bin";
    FILE* file = fopen(filename, "wb");
    fwrite("test data", 1, 9, file);
    fclose(file);

    // Open the test file with FileWrapper
    FileWrapper fileWrapper(filename, "rb");

    // Read the data from the file
    char buffer[10] = {};
    bool success = fileWrapper.Read(buffer, 9);

    // Check that the read was successful and that the buffer contains the correct data
    EXPECT_TRUE(success);
    EXPECT_STREQ(buffer, "test data");

    // Delete the test file
    std::remove(filename);
}
