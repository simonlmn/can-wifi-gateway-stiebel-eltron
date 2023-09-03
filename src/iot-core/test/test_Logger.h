#include <yatest/TestSuite.h>
#include <yatest/Mocks.h>

#include <iostream>
#include <sstream>

#include "../src/Logger.h"

namespace {
  template<typename TestCaseFunction>
  std::function<void()> testLogger(TestCaseFunction testCase, std::string expected) {
    return [=] () {
      iot_core::Time time;
      iot_core::Logger logger {time};

      testCase(logger, time);

      std::stringstream output;
      logger.output([&](const char* entry){ output << entry; });
      yatest::expect(output.str() == expected, output.str().c_str());
    };
  }

  static const yatest::TestSuite& TestLogger =
  yatest::suite("Logger")
    .tests("log plain message without level", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.log("test", "message 1");
    }, "[0e0w0d00h00m00s000|test|   ] message 1\n"))
    .tests("log multiple plain messages without level", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.log("test", "message 1");
      advanceTimeMs(123); time.update();
      logger.log("test", "message 2");
    }, "[0e0w0d00h00m00s000|test|   ] message 1\n[0e0w0d00h00m00s123|test|   ] message 2\n"))
    .tests("log plain message with error level", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.log(iot_core::LogLevel::Error, "test", "message 1");
    }, "[0e0w0d00h00m00s000|test|ERR] message 1\n"))
    .tests("log plain message with info level", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.log(iot_core::LogLevel::Info, "test", "message 1");
    }, "[0e0w0d00h00m00s000|test|INF] message 1\n"))
    .tests("plain message with debug level not logged by default", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.log(iot_core::LogLevel::Debug, "test", "message 1");
    }, ""))
    .tests("log plain message with debug level", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.logLevel("test", iot_core::LogLevel::Debug);
      logger.log(iot_core::LogLevel::Debug, "test", "message 1");
    }, "[0e0w0d00h00m00s000|test|DBG] message 1\n"))
    .tests("log message function", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      logger.log(iot_core::LogLevel::Info, "test", [] () { return "message 1"; });
    }, "[0e0w0d00h00m00s000|test|INF] message 1\n"))
    .tests("log message function not called if level to high", testLogger([] (iot_core::Logger& logger, iot_core::Time& time) {
      bool functionCalled = false;
      logger.log(iot_core::LogLevel::Trace, "test", [&] () { functionCalled = true; return "message 1"; });
      yatest::expect(!functionCalled, "message function should not be called");
    }, ""));
}
