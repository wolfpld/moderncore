#include <gtest/gtest.h>

#include "util/Logs.hpp"

TEST(LogsTest, MCoreLogMessage) {
    testing::internal::CaptureStdout();

    // Set the log level to LogLevel::Debug
    SetLogLevel(LogLevel::Debug);

    // Call MCoreLogMessage with various log levels and messages
    MCoreLogMessage(LogLevel::Debug, "test.cpp", 42, "Debug message");
    MCoreLogMessage(LogLevel::Info, "test.cpp", 42, "Info message");
    MCoreLogMessage(LogLevel::Warning, "test.cpp", 42, "Warning message");
    MCoreLogMessage(LogLevel::Error, "test.cpp", 42, "Error message");
    MCoreLogMessage(LogLevel::Fatal, "test.cpp", 42, "Fatal message");

    // Capture the output of MCoreLogMessage
    std::string output = testing::internal::GetCapturedStdout();

    // Check that the output contains the correct log messages
    EXPECT_NE(output.find("Debug message"), std::string::npos);
    EXPECT_NE(output.find("Info message"), std::string::npos);
    EXPECT_NE(output.find("Warning message"), std::string::npos);
    EXPECT_NE(output.find("Error message"), std::string::npos);
    EXPECT_NE(output.find("Fatal message"), std::string::npos);
}
