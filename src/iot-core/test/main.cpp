
#include <yatest/TestSuite.h>
#include <yatest/TestRunner.h>

// Include all individual test suites
#include "test_JsonWriter.h"
#include "test_Logger.h"

int main() {
  return yatest::run();
}
