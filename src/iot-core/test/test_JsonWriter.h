#include <yatest/TestSuite.h>
#include <yatest/Mocks.h>

#include <iostream>
#include <sstream>

#include "../src/JsonWriter.h"

namespace {
  struct Out {
    std::stringstream output;

    size_t write(char c) { output << c; return 1; }
    size_t write(const char* str) { output << str; return strlen(str); }
  };

  template<typename TestCaseFunction>
  std::function<void()> validJson(TestCaseFunction testCase, std::string expected) {
    return [=] () {
      Out o;
      bool failed = false;
      {
        auto writer = iot_core::makeJsonWriter(o);
        testCase(writer);
        failed = writer.failed();
      }
      yatest::expect(!failed, "writer failed");
      yatest::expect(o.output.str() == expected, "output != expected");
    };
  }

  template<typename TestCaseFunction>
  std::function<void()> invalidJson(TestCaseFunction testCase) {
    return [=] () {
      Out o;
      bool failed = false;
      {
        auto writer = iot_core::makeJsonWriter(o);
        testCase(writer);
        failed = writer.failed();
      }
      yatest::expect(failed, "writer was expected to fail");
    };
  }

  using JsonWriter = iot_core::JsonWriter<Out>;

  static const yatest::TestSuite& TestJsonWriter =
  yatest::suite("JsonWriter")
    .tests("empty document", validJson([] (JsonWriter& writer) {
        writer.end();
    }, ""))
    .tests("plain value", validJson([] (JsonWriter& writer) {
        writer.plainValue("123");
    }, "123"))
    .tests("string value", validJson([] (JsonWriter& writer) {
        writer.stringValue("123");
    }, "\"123\""))
    .tests("empty object", validJson([] (JsonWriter& writer) {
        writer.openObject();
    }, "{}"))
    .tests("empty list", validJson([] (JsonWriter& writer) {
        writer.openList();
    }, "[]"))
    .tests("list with plain value", validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.plainValue("123");
    }, "[123]"))
    .tests("list with plain and string value", validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.plainValue("123");
        writer.stringValue("123");
    }, "[123,\"123\"]"))
    .tests("list with plain, string and nested list value", validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.plainValue("123");
        writer.stringValue("123");
        writer.openList();
    }, "[123,\"123\",[]]"))
    .tests("list with empty object value", validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.openObject();
    }, "[{}]"))
    .tests("list with multiple nested empty list values", validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.openList();
        writer.close();
        writer.openList();
        writer.close();
        writer.openList();
    }, "[[],[],[]]"))
    .tests("object with plain value property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.propertyPlain("prop", "123");
    }, "{\"prop\":123}"))
    .tests("object with string value property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.propertyString("prop", "123");
    }, "{\"prop\":\"123\"}"))
    .tests("object with plain value property (separate construction)", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.plainValue("123");
    }, "{\"prop\":123}"))
    .tests("object with empty list property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openList();
    }, "{\"prop\":[]}"))
    .tests("object with non-empty list property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openList();
        writer.plainValue("123");
        writer.plainValue("234");
    }, "{\"prop\":[123,234]}"))
    .tests("object with empty object property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
    }, "{\"prop\":{}}"))
    .tests("object with another property after an empty object property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
        writer.close();
        writer.propertyPlain("prop2", "123");
    }, "{\"prop\":{},\"prop2\":123}"))
    .tests("object with non-empty object property", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
        writer.propertyPlain("prop2", "123");
    }, "{\"prop\":{\"prop2\":123}}"))
    .tests("object with multiple plain value properties", validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.propertyPlain("prop1", "123");
        writer.propertyPlain("prop2", "true");
        writer.propertyPlain("prop3", "1.2");
    }, "{\"prop1\":123,\"prop2\":true,\"prop3\":1.2}"))


    .tests("top-level multiple plain values are not allowed", invalidJson([] (JsonWriter& writer) {
        writer.plainValue("123");
        writer.plainValue("123");
    }))
    .tests("top-level string and plain values are not allowed", invalidJson([] (JsonWriter& writer) {
        writer.stringValue("123");
        writer.plainValue("123");
    }))
    .tests("string value within an object (without property) is not allowed", invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.stringValue("123");
    }))
    .tests("list within an object (without property) is not allowed", invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.openList();
    }))
    .tests("top-level object and plain value is not allowed", invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.close();
        writer.plainValue("123");
    }))
    .tests("object with unfinished property is not allowed", invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.close();
    }))
    .tests("top-level property is not allowed", invalidJson([] (JsonWriter& writer) {
        writer.property("prop");
    }))
    .tests("list with property is not allowed", invalidJson([] (JsonWriter& writer) {
        writer.openList();
        writer.property("prop");
    }));
}
