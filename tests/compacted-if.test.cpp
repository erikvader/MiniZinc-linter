#include "test_common.hpp"

TEST_CASE("compacted if", "[rule]") {
  LZN_TEST_CASE_INIT(20);

  SECTION("good case 1") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint if a = 1 then b else 0 endif = 0;");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 39));
  }

  SECTION("good case 1 float") {
    LZN_MODEL("var float: a;\n"
              "var float: b;\n"
              "constraint if a = 1.0 then b else 0.0 endif = 0;");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 43));
  }

  SECTION("good case 1 inverted") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint if a = 1 then 0 else b endif = 0;");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 39));
  }

  SECTION("both zero") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint if a = 1 then 0 else 0 endif = 0;");
    LZN_EXPECTED();
  }

  SECTION("both non-zero") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint if a = 1 then b else b endif = 0;");
    LZN_EXPECTED();
  }

  SECTION("no else") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint if a = 1 then b = 0 endif;");
    LZN_EXPECTED();
  }

  SECTION("on par") {
    LZN_MODEL("int: a;\n"
              "int: b;\n"
              "constraint if a = 1 then b else 0 endif = 7;");
    LZN_EXPECTED();
  }

  SECTION("if-part is par") {
    LZN_MODEL("var int: y;\n"
              "int: b = 1;\n"
              "constraint if b = 1 then y else 0 endif = 7;");
    LZN_EXPECTED();
  }

  LZN_TEST_CASE_END
}
