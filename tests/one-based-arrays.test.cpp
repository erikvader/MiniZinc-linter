#include "test_common.hpp"

TEST_CASE("one based arrays", "[rule]") {
  LZN_TEST_CASE_INIT(19);

  SECTION("okay array") {
    LZN_MODEL("array[1..5] of var int: xs;");
    LZN_EXPECTED();
  }

  SECTION("okay array 2") {
    LZN_MODEL("set of int: ns = 1..5;"
              "array[ns] of var int: xs;");
    LZN_EXPECTED();
  }

  SECTION("okay array 3") {
    LZN_MODEL("int: K = 7;"
              "set of int: ns = 1..K;"
              "array[ns] of var int: xs;");
    LZN_EXPECTED();
  }

  SECTION("okay array 4") {
    LZN_MODEL("array[{1,2,3}] of var int: xs;");
    LZN_EXPECTED();
  }

  SECTION("okay array 5") {
    LZN_MODEL("array[1..5, 1..2, 1..7] of var int: xs;");
    LZN_EXPECTED();
  }

  SECTION("bad array") {
    LZN_MODEL("array[2..5] of var int: xs;");
    LZN_EXPECTED(LZN_ONELINE(1, 7, 10));
  }

  SECTION("bad array 2") {
    LZN_MODEL("set of int: ns = 4..5;\n"
              "array[ns] of var int: xs;");
    LZN_EXPECTED(LZN_ONELINE(2, 7, 8));
  }

  SECTION("bad array 3") {
    LZN_MODEL("int: K = 7;\n"
              "set of int: ns = K..K+5;\n"
              "array[ns] of var int: xs;");
    LZN_EXPECTED(LZN_ONELINE(3, 7, 8));
  }

  SECTION("bad array 4") {
    LZN_MODEL("array[{2,3,4}] of var int: xs;");
    LZN_EXPECTED(LZN_ONELINE(1, 7, 13));
  }

  SECTION("bad array 5") {
    LZN_MODEL("array[2..5, 2..2, 2..7] of var int: xs;");
    LZN_EXPECTED(LZN_ONELINE(1, 7, 10), LZN_ONELINE(1, 13, 16), LZN_ONELINE(1, 19, 22));
  }

  LZN_TEST_CASE_END
}
