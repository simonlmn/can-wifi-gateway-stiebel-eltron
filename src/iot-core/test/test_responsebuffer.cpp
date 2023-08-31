#ifndef ARDUINO

#include "../ResponseBuffer.h"
#include <cassert>
#include <string>
#include <vector>

struct ServerMock {
  int _code {0};
  std::string _contentType {};
  std::vector<std::string> _sentContent {};
  bool _finalized {false};

  bool chunkedResponseModeStart(int code, const char* contentType) {
    _code = code;
    _contentType = contentType;
    return true;
  }

  void sendContent(const char* content, size_t size) {
    _sentContent.push_back(std::string{content, size});
  }

  void chunkedResponseFinalize() { _finalized = true; }
};

void test_ResponseBuffer() {
  using ResponseBuffer = ResponseBuffer<ServerMock, 10u>;
  
  {
    // Send nothing
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);
    assert(buffer.size() == 0u);
    assert(server._code == 123u);
    assert(server._contentType == "text/plain");
    assert(server._sentContent.empty());

    buffer.end();
    assert(buffer.size() == 0u);
    assert(server._sentContent.empty());
    assert(server._finalized == true);
  }

  {
    // Send data until buffer is exactly full
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.plainText("test1");
    assert(buffer.size() == 5);
    assert(server._sentContent.empty());

    buffer.plainText("test2");
    assert(buffer.size() == 10);
    assert(server._sentContent.empty());

    buffer.end();
    assert(buffer.size() == 0u);
    assert(server._sentContent.back() == "test1test2");
    assert(server._finalized == true);
  }

  {
    // Send data larger than buffer capacity
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.plainText("12345678910");
    assert(buffer.size() == 1);
    assert(server._sentContent.back() == "1234567891");

    buffer.end();
    assert(buffer.size() == 0u);
    assert(server._sentContent.back() == "0");
    assert(server._finalized == true);
  }

  {
    // Send data to fill buffer multiple times
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.plainText("foo");
    assert(buffer.size() == 3);
    buffer.plainText("bar");
    assert(buffer.size() == 6);
    buffer.plainText("baz");
    assert(buffer.size() == 9);
    buffer.plainText("xyz");
    assert(buffer.size() == 2);
    assert(server._sentContent.back() == "foobarbazx");

    buffer.plainText("qwe");
    assert(buffer.size() == 5);
    buffer.plainText("asd");
    assert(buffer.size() == 8);
    buffer.plainText("iop");
    assert(buffer.size() == 1);   
    assert(server._sentContent.back() == "yzqweasdio");
    
    buffer.plainText("jkl");
    assert(buffer.size() == 4);

    buffer.end();
    assert(buffer.size() == 0u);
    assert(server._sentContent.back() == "pjkl");
    assert(server._finalized == true);
  }

  {
    // Flush data explicitly
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.plainText("bar");
    assert(buffer.size() == 3);

    buffer.flush();
    assert(buffer.size() == 0u);
    assert(server._sentContent.back() == "bar");
    assert(server._finalized == false);

    buffer.end();
    assert(server._finalized == true);
  }

  {
    // Clear buffer
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.plainText("bar");
    assert(buffer.size() == 3);

    buffer.clear();
    assert(buffer.size() == 0u);
    assert(server._sentContent.empty());
    assert(server._finalized == false);

    buffer.end();
    assert(server._sentContent.empty());
    assert(server._finalized == true);
  }

  {
    // Clear buffer with new content afterwards
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.plainText("foo");
    assert(buffer.size() == 3);

    buffer.clear();
    assert(buffer.size() == 0u);
    assert(server._sentContent.empty());
    assert(server._finalized == false);

    buffer.plainText("bar");
    assert(buffer.size() == 3);

    buffer.end();
    assert(server._sentContent.back() == "bar");
    assert(server._finalized == true);
  }

  {
    // JSON object with raw property
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.jsonObjectOpen();
    buffer.jsonPropertyRaw("a", "1");
    buffer.jsonObjectClose();
    
    buffer.end();
    assert(server._sentContent.back() == "{\"a\":1}");
  }

  {
    // JSON object with string property
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.jsonObjectOpen();
    buffer.jsonPropertyString("a", "1");
    buffer.jsonObjectClose();
    
    buffer.end();
    assert(server._sentContent.back() == "{\"a\":\"1\"}");
  }

  {
    // JSON object with list property with two string values
    ServerMock server;
    ResponseBuffer buffer {server};

    assert(buffer.begin(123u, "text/plain") == true);

    buffer.jsonObjectOpen();
    buffer.jsonPropertyStart("a");
    buffer.jsonListOpen();
    buffer.jsonString("b");
    buffer.jsonSeparator();
    buffer.jsonString("c");
    buffer.jsonListClose();
    buffer.jsonObjectClose();
    
    buffer.end();
    assert(server._sentContent[0] == "{\"a\":[\"b\",");
    assert(server._sentContent[1] == "\"c\"]}");
  }
}

int main() {
  test_ResponseBuffer();
  return 0;
}

#endif
