#include "test_common.hpp"

TEST_CASE("variable in generator", "[rule]") {
  LZN_TEST_CASE_INIT(7);

  SECTION("no var generator") {
    LZN_MODEL("constraint forall(i in 1..2)(true);");
    LZN_EXPECTED();
  }

  SECTION("var generator") {
    LZN_MODEL("var int: x;\n"
              "constraint forall(i in 1..x)(true);");
    LZN_EXPECTED(LZN_ONELINE(2, 24, 27));
  }

  SECTION("var generator as variable") {
    LZN_MODEL("var int: x;\n"
              "var set of int: xs = 1..x;\n"
              "constraint forall(i in xs)(true);");
    LZN_EXPECTED(LZN_ONELINE(3, 24, 25));
  }

  LZN_TEST_CASE_END
}
