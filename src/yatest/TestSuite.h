#ifndef YATEST_TESTSUITE_H_
#define YATEST_TESTSUITE_H_

#include <vector>
#include <functional>
#include <memory>
#include <string>

namespace yatest {

enum struct TestStatus {
  Passed,
  Failed
};

struct TestResult final {
  const char* name;
  TestStatus status;
  std::string what;

  TestResult(const char* name, TestStatus status, std::string what) : name(name), status(status), what(what) {}
};

class TestSuiteResult final {
  std::vector<TestResult> _testResults {};

public:
  const std::vector<TestResult>& testResults() const { return _testResults; }

  void passed(const char* name) {
    _testResults.emplace_back(name, TestStatus::Passed, "");
  }

  void failed(const char* name) {
    failed(name, "");
  }

  void failed(const char* name, const char* what) {
    _testResults.emplace_back(name, TestStatus::Failed, what);
  }
};

struct ITestSuite {
  virtual ~ITestSuite() {}
  virtual const char* name() const = 0;
  virtual TestSuiteResult run() = 0;
};

class TestSuite final : public ITestSuite {
  const char* _name;
  std::vector<std::pair<const char*, std::function<void()>>> _tests {};

public:
  TestSuite(const char* name) : _name(name) {}

  TestSuite& tests(const char* name, std::function<void()> test) {
    _tests.emplace_back(std::make_pair(name, test));
    return *this;
  }

  const char* name() const override {
    return _name;
  }

  TestSuiteResult run() override {
    TestSuiteResult result;
    for (auto [name, test] : _tests) {
      try {
        test();
        result.passed(name);
      } catch (std::exception& e) {
        result.failed(name, e.what());
      } catch (...) {
        result.failed(name);
      }
    }
    return std::move(result);
  }
};

std::vector<std::unique_ptr<ITestSuite>> TestSuites = {};

TestSuite& suite(const char* name) {
  return *static_cast<TestSuite*>(TestSuites.emplace_back(std::make_unique<TestSuite>(name)).get());
}

struct ExpectationFailed final : public std::logic_error {
  using logic_error::logic_error;
};

void expect(bool expectation, const char* what) { if (!expectation) throw ExpectationFailed(what); }

}

#endif
