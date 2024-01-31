#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "util/Config.hpp"

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create the test directory and subdirectory
        std::filesystem::create_directories("./test/ModernCore");

        // Create a mock test.ini file
        std::ofstream file("./test/ModernCore/test.ini");
        file << "[section1]\n";
        file << "key1=value1\n";
        file << "[section2]\n";
        file << "key2=456\n";
        file.close();
    }

    void TearDown() override {
        // Remove the mock test.ini file
        std::remove("./test/ModernCore/test.ini");
    }
};

TEST_F(ConfigTest, GetString) {
    // Create a test config object
    Config config("./test/ModernCore/test.ini");

    // Check that GetString returns the default value when the section/key is not found
    EXPECT_STREQ(config.Get<const char*>("nonexistent_section", "nonexistent_key", "default"), "default");

    // Check that GetString returns the correct value when the section/key is found
    EXPECT_STREQ(config.Get<const char*>("section1", "key1", "default"), "value1");
}

TEST_F(ConfigTest, GetInt) {
    // Create a test config object
    Config config("./test/ModernCore/test.ini");

    // Check that GetInt returns the default value when the section/key is not found
    EXPECT_EQ(config.Get<int>("nonexistent_section", "nonexistent_key", 123), 123);

    // Check that GetInt returns the correct value when the section/key is found
    EXPECT_EQ(config.Get<int>("section2", "key2", 123), 456);
}
