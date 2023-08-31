#ifndef ARDUINO

#include "Mocks.h"
#include "../JsonWriter2.h"
#include <iostream>
#include <sstream>

struct Out {
    std::stringstream output;

    size_t write(char c) { output << c; return 1; }
    size_t write(const char* str) { output << str; return strlen(str); }
};

template<typename TestCaseFunction>
void expect(TestCaseFunction testCase, std::string expected) {
    Out o;
    bool failed = false;
    {
        auto writer = iot_core::makeJsonWriter2(o);
        testCase(writer);
        failed = writer.failed();
    }
    std::cout << (failed ? "FAIL (writer failed)" : (o.output.str() == expected ? "PASS" : "FAIL (output != expected)")) << ": " << o.output.str() << std::endl;
}

template<typename TestCaseFunction>
void expect_failed(TestCaseFunction testCase) {
    Out o;
    bool failed = false;
    {
        auto writer = iot_core::makeJsonWriter2(o);
        testCase(writer);
        failed = writer.failed();
    }
    std::cerr << (failed ? "PASS (did fail as expected)" : "FAIL (did not fail as expected)") << ": " << o.output.str() << std::endl;
}

int main() {
    using JsonWriter = iot_core::JsonWriter2<Out>;

    expect([] (JsonWriter& writer) {
        writer.end();
    }, "");
    expect([] (JsonWriter& writer) {
        writer.plainValue("123");
    }, "123");
    expect([] (JsonWriter& writer) {
        writer.stringValue("123");
    }, "\"123\"");
    expect([] (JsonWriter& writer) {
        writer.openObject();
    }, "{}");
    expect([] (JsonWriter& writer) {
        writer.openList();
    }, "[]");
    expect([] (JsonWriter& writer) {
        writer.openList();
        writer.plainValue("123");
    }, "[123]");
    expect([] (JsonWriter& writer) {
        writer.openList();
        writer.plainValue("123");
        writer.stringValue("123");
    }, "[123,\"123\"]");
    expect([] (JsonWriter& writer) {
        writer.openList();
        writer.plainValue("123");
        writer.stringValue("123");
        writer.openList();
    }, "[123,\"123\",[]]");
    expect([] (JsonWriter& writer) {
        writer.openList();
        writer.openObject();
    }, "[{}]");
    expect([] (JsonWriter& writer) {
        writer.openList();
        writer.openList();
        writer.close();
        writer.openList();
        writer.close();
        writer.openList();
    }, "[[],[],[]]");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.propertyPlain("prop", "123");
    }, "{\"prop\":123}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.propertyString("prop", "123");
    }, "{\"prop\":\"123\"}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.plainValue("123");
    }, "{\"prop\":123}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openList();
    }, "{\"prop\":[]}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openList();
        writer.plainValue("123");
        writer.plainValue("234");
    }, "{\"prop\":[123,234]}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
    }, "{\"prop\":{}}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
        writer.close();
        writer.propertyPlain("prop2", "123");
    }, "{\"prop\":{},\"prop2\":123}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
        writer.propertyPlain("prop2", "123");
    }, "{\"prop\":{\"prop2\":123}}");
    expect([] (JsonWriter& writer) {
        writer.openObject();
        writer.propertyPlain("prop1", "123");
        writer.propertyPlain("prop2", "true");
        writer.propertyPlain("prop3", "1.2");
    }, "{\"prop1\":123,\"prop2\":true,\"prop3\":1.2}");



    expect_failed([] (JsonWriter& writer) {
        writer.plainValue("123");
        writer.plainValue("123");
    });
    expect_failed([] (JsonWriter& writer) {
        writer.stringValue("123");
        writer.plainValue("123");
    });
    expect_failed([] (JsonWriter& writer) {
        writer.openObject();
        writer.stringValue("123");
    });
    expect_failed([] (JsonWriter& writer) {
        writer.openObject();
        writer.openList();
    });
    expect_failed([] (JsonWriter& writer) {
        writer.openObject();
        writer.close();
        writer.plainValue("123");
    });
    expect_failed([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.close();
    });
    expect_failed([] (JsonWriter& writer) {
        writer.property("prop");
    });
    expect_failed([] (JsonWriter& writer) {
        writer.openList();
        writer.property("prop");
    });
    return 0;
}

#endif
