#ifndef ARDUINO

#include "../Utils.h"
#include <cassert>
#include <string>

void test_ConstString_char() {
  char dest[10] = {'\0'};

  {
    // Empty string
    ConstString<char> empty {""};

    // Length
    assert(empty.len() == 0);

    // Copy
    assert(empty.copy(dest, 10, 0) == 0);
    assert(strcmp(dest, "") == 0);
  }
  {
    // Non-empty string
    ConstString<char> test {"Test"};

    // Length
    assert(test.len() == 4);
    assert(test.len(2) == 2);
    assert(test.len(4) == 0);

    // Without offset
    assert(test.copy(dest, 10, 0) == 4);
    assert(strcmp(dest, "Test") == 0);
    assert(test.copy(dest, 4, 0) == 4);
    assert(strcmp(dest, "Test") == 0);
    assert(test.copy(dest, 2, 0) == 4);
    assert(strcmp(dest, "Te") == 0);
    assert(test.copy(dest, 0, 0) == 4);
    assert(strcmp(dest, "") == 0);

    // With offset
    assert(test.copy(dest, 10, 1) == 3);
    assert(strcmp(dest, "est") == 0);
    assert(test.copy(dest, 3, 1) == 3);
    assert(strcmp(dest, "est") == 0);
    assert(test.copy(dest, 2, 1) == 3);
    assert(strcmp(dest, "es") == 0);
    assert(test.copy(dest, 1, 1) == 3);
    assert(strcmp(dest, "e") == 0);
    assert(test.copy(dest, 0, 1) == 3);
    assert(strcmp(dest, "") == 0);

    assert(test.copy(dest, 10, 4) == 0);
    assert(strcmp(dest, "") == 0);
  }
}

void test_ConstString_FlashStringHelper() {
  char dest[10] = {'\0'};

  {
    // Empty string
    ConstString<__FlashStringHelper> empty {F("")};

    // Length
    assert(empty.len() == 0);

    // Copy
    assert(empty.copy(dest, 10, 0) == 0);
    assert(strcmp(dest, "") == 0);
  }
  {
    // Non-empty string
    ConstString<__FlashStringHelper> test {F("Test")};

    // Length
    assert(test.len() == 4);
    assert(test.len(2) == 2);
    assert(test.len(4) == 0);

    // Without offset
    assert(test.copy(dest, 10, 0) == 4);
    assert(strcmp(dest, "Test") == 0);
    assert(test.copy(dest, 4, 0) == 4);
    assert(strcmp(dest, "Test") == 0);
    assert(test.copy(dest, 2, 0) == 4);
    assert(strcmp(dest, "Te") == 0);
    assert(test.copy(dest, 0, 0) == 4);
    assert(strcmp(dest, "") == 0);

    // With offset
    assert(test.copy(dest, 10, 1) == 3);
    assert(strcmp(dest, "est") == 0);
    assert(test.copy(dest, 3, 1) == 3);
    assert(strcmp(dest, "est") == 0);
    assert(test.copy(dest, 2, 1) == 3);
    assert(strcmp(dest, "es") == 0);
    assert(test.copy(dest, 1, 1) == 3);
    assert(strcmp(dest, "e") == 0);
    assert(test.copy(dest, 0, 1) == 3);
    assert(strcmp(dest, "") == 0);

    assert(test.copy(dest, 10, 4) == 0);
    assert(strcmp(dest, "") == 0);
  }
}

void test_toString() {
  const char* str;

  {
    // Single character
    str = toString('a');
    assert(strlen(str) == 1);
    assert(strcmp(str, "a") == 0);
  }
  {
    // Decimal (base 10)
    str = toString(uint8_t(123u), 10);
    assert(strlen(str) == 3);
    assert(strcmp(str, "123") == 0);

    str = toString(int8_t(-123u), 10);
    assert(strlen(str) == 4);
    assert(strcmp(str, "-123") == 0);

    str = toString(uint16_t(12345u), 10);
    assert(strlen(str) == 5);
    assert(strcmp(str, "12345") == 0);

    str = toString(int16_t(-12345u), 10);
    assert(strlen(str) == 6);
    assert(strcmp(str, "-12345") == 0);

    str = toString(uint32_t(12345678u), 10);
    assert(strlen(str) == 8);
    assert(strcmp(str, "12345678") == 0);

    str = toString(int32_t(-12345678u), 10);
    assert(strlen(str) == 9);
    assert(strcmp(str, "-12345678") == 0);

    str = toString(12345678ul, 10);
    assert(strlen(str) == 8);
    assert(strcmp(str, "12345678") == 0);

    str = toString(-12345678l, 10);
    assert(strlen(str) == 9);
    assert(strcmp(str, "-12345678") == 0);
  }
  {
    // Hexadecimal (base 16)
    str = toString(uint8_t(123u), 16);
    assert(strlen(str) == 2);
    assert(strcmp(str, "7B") == 0);

    str = toString(uint16_t(12345u), 16);
    assert(strlen(str) == 4);
    assert(strcmp(str, "3039") == 0);

    str = toString(uint32_t(12345678u), 16);
    assert(strlen(str) == 6);
    assert(strcmp(str, "BC614E") == 0);

    str = toString(12345678ul, 16);
    assert(strlen(str) == 6);
    assert(strcmp(str, "BC614E") == 0);
  }
}

int main() {
  test_ConstString_char();
  test_ConstString_FlashStringHelper();
  test_toString();
  return 0;
}

#endif
