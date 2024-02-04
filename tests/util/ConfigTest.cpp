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
    EXPECT_STREQ(config.Get("nonexistent_section", "nonexistent_key", "default"), "default");

    // Check that GetString returns the correct value when the section/key is found
    EXPECT_STREQ(config.Get("section1", "key1", "default"), "value1");
}

TEST_F(ConfigTest, GetInt) {
    // Create a test config object
    Config config("./test/ModernCore/test.ini");

    // Check that GetInt returns the default value when the section/key is not found
    EXPECT_EQ(config.Get("nonexistent_section", "nonexistent_key", 123), 123);

    // Check that GetInt returns the correct value when the section/key is found
    EXPECT_EQ(config.Get("section2", "key2", 123), 456);
}

TEST_F(ConfigTest, GetOptString) {
    // Create a test config object
    Config config("./test/ModernCore/test.ini");

    // Check that GetOptString returns false when the section/key is not found
    const char* output;
    EXPECT_FALSE(config.GetOpt("nonexistent_section", "nonexistent_key", output));

    // Check that GetOptString returns true and the correct value when the section/key is found
    EXPECT_TRUE(config.GetOpt("section1", "key1", output));
    EXPECT_STREQ(output, "value1");
}

TEST_F(ConfigTest, GetOptInt) {
    // Create a test config object
    Config config("./test/ModernCore/test.ini");

    // Check that GetOptInt returns false when the section/key is not found
    int output;
    EXPECT_FALSE(config.GetOpt("nonexistent_section", "nonexistent_key", output));

    // Check that GetOptInt returns true and the correct value when the section/key is found
    EXPECT_TRUE(config.GetOpt("section2", "key2", output));
    EXPECT_EQ(output, 456);
}

TEST_F(ConfigTest, LoadConfigFile) {
    // Create a test config object
    Config config("./test/ModernCore/test.ini");

    // Check that the config file is loaded correctly
    const char* output;
    EXPECT_TRUE(config.GetOpt("section1", "key1", output));
    EXPECT_STREQ(output, "value1");
    int outputInt;
    EXPECT_TRUE(config.GetOpt("section2", "key2", outputInt));
    EXPECT_EQ(outputInt, 456);
}

TEST_F(ConfigTest, LoadNonExistentConfigFile) {
    // Create a test config object with a non-existent file
    Config config("./test/ModernCore/nonexistent.ini");

    // Check that the config file is not loaded
    const char* output;
    EXPECT_FALSE(config.GetOpt("section1", "key1", output));
    int outputInt;
    EXPECT_FALSE(config.GetOpt("section2", "key2", outputInt));
}

TEST_F(ConfigTest, LoadConfigFileFromEnvironmentVariable) {
    // Set the XDG_CONFIG_HOME environment variable
    setenv("XDG_CONFIG_HOME", "./test", 1);

    // Create a test config object
    Config config("test.ini");

    // Check that the config file is loaded correctly
    const char* output;
    EXPECT_TRUE(config.GetOpt("section1", "key1", output));
    EXPECT_STREQ(output, "value1");
    int outputInt;
    EXPECT_TRUE(config.GetOpt("section2", "key2", outputInt));
    EXPECT_EQ(outputInt, 456);

    // Unset the XDG_CONFIG_HOME environment variable
    unsetenv("XDG_CONFIG_HOME");
}
