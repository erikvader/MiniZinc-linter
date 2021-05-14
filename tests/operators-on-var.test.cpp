#include "test_common.hpp"

TEST_CASE("operators on var", "[rule]") {
  LZN_TEST_CASE_INIT(18);

  SECTION("not par") {
    LZN_MODEL("var bool: a = not true;");
    LZN_EXPECTED();
  }

  SECTION("not var") {
    LZN_MODEL("var bool: b;\n"
              "var bool: a = not b;");
    LZN_EXPECTED(LZN_ONELINE(2, 15, 17));
  }

  SECTION("divide in constraint") {
    LZN_MODEL("var int: b;\n"
              "var int: a;\n"
              "constraint a = b div 2");
    LZN_EXPECTED(LZN_ONELINE(3, 16, 22));
  }

  SECTION("divide in constraint swapped") {
    LZN_MODEL("var int: b;\n"
              "var int: a;\n"
              "constraint a = 2 div b");
    LZN_EXPECTED(LZN_ONELINE(3, 16, 22));
  }

  SECTION("or") {
    LZN_MODEL("var int: b;\n"
              "var int: a;\n"
              "constraint (a = 1) \\/ (b = 1);");
    LZN_EXPECTED(LZN_ONELINE(3, 17, 24));
  }

  SECTION("divide par") {
    LZN_MODEL("int: b;\n"
              "int: a = b div 5;");
    LZN_EXPECTED();
  }

  LZN_TEST_CASE_END
}
