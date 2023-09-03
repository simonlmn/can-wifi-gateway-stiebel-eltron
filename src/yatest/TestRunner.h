#ifndef YATEST_TESTRUNNER_H_
#define YATEST_TESTRUNNER_H_

#include "TestSuite.h"
#include <iostream>

namespace yatest {

int run() {
  size_t totalPassed = 0u;
  size_t totalFailed = 0u;
  
  for (auto& suite : yatest::TestSuites) {
    std::cout << "Running " << suite->name() << " [" << std::endl;
    auto result = suite->run();
    for (auto& testResult : result.testResults()) {
      switch (testResult.status)
      {
      case yatest::TestStatus::Passed:
        totalPassed += 1u;
        std::cout << "  PASS " << testResult.name << std::endl;
        break;
      case yatest::TestStatus::Failed:
        totalFailed += 1u;
        std::cout << "  FAIL " << testResult.name << " (" << testResult.what << ")" << std::endl;
        break;
      }
    }
    std::cout << "]" << std::endl;
  }

  std::cout << "\nTotal: " << totalPassed << " passed, " << totalFailed  << " failed" << std::endl;

  return totalFailed;
}

}

#endif
