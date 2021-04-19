#include "test_common.hpp"

TEST_CASE("0..1 variables", "[rule]") {
  LZN_TEST_CASE_INIT(22);

  SECTION("simple imply 0") {
    LZN_MODEL("var 0..1: a;\n"
              "var 0..1: b;\n"
              "constraint a = 0 -> b = 0;");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 25));
  }

  SECTION("simple imply 1") {
    LZN_MODEL("var 0..1: a;\n"
              "var 0..1: b;\n"
              "constraint a = 1 -> b = 1;");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 25));
  }

  SECTION("simple imply wrong constants") {
    LZN_MODEL("var 0..1: a;\n"
              "var 0..1: b;\n"
              "constraint a = 1 -> b = 0;\n"
              "constraint a = 0 -> b = 1;\n");
    LZN_EXPECTED();
  }

  SECTION("simple imply not equals") {
    LZN_MODEL("var 0..1: a;\n"
              "var 0..1: b;\n"
              "constraint a > 1 -> b = 1;\n"
              "constraint a = 0 -> b <= 0;\n");
    LZN_EXPECTED();
  }

  SECTION("simple imply wrong domains") {
    LZN_MODEL("var 0..2: a;\n"
              "var 0..1: b;\n"
              "constraint a = 1 -> b = 1;\n");
    LZN_EXPECTED();
  }

  SECTION("simple imply no domains") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint a = 1 -> b = 1;\n");
    LZN_EXPECTED();
  }

  SECTION("simple imply offset domains") {
    LZN_MODEL("var 0..1: a;\n"
              "var 1..2: b;\n"
              "constraint a = 1 -> (b-1) = 1;\n");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 29));
  }

  SECTION("simple imply array") {
    LZN_MODEL("array[1..2] of var 0..1: as;\n"
              "constraint as[1] = 1 -> as[2] = 1;\n");
    LZN_EXPECTED(LZN_ONELINE(2, 12, 33));
  }

  SECTION("simple imply array in forall") {
    LZN_MODEL("array[1..2] of var 0..1: as;\n"
              "constraint forall(i in 1..1)(as[i] = 1 -> as[i+1] = 1);\n");
    LZN_EXPECTED(LZN_ONELINE(2, 30, 53));
  }

  SECTION("simple imply array in forall wrong domain") {
    LZN_MODEL("array[1..2] of var -1..1: as;\n"
              "constraint forall(i in 1..1)(as[i] = 1 -> as[i+1] = 1);\n");
    LZN_EXPECTED();
  }

  SECTION("sum array") {
    LZN_MODEL("array[1..5] of var 0..1: as;\n"
              "constraint 0 = sum(i in 1..5)(as[i] = 1);");
    LZN_EXPECTED(LZN_ONELINE(2, 16, 40));
  }

  SECTION("sum array no domain") {
    LZN_MODEL("array[1..5] of var int: as;\n"
              "constraint 0 = sum(i in 1..5)(as[i] = 1);");
    LZN_EXPECTED();
  }

  SECTION("sum array not covering everything") {
    LZN_MODEL("array[1..5] of var 0..1: as;\n"
              "constraint 0 = sum(i in 1..3)(as[i] = 1);");
    LZN_EXPECTED();
  }

  SECTION("sum array typo") {
    LZN_MODEL("array[1..5] of var 0..1: as;\n"
              "constraint 0 = sum(i in 1..5)(as[1] = 1);");
    LZN_EXPECTED();
  }

  SECTION("sum array offset") {
    LZN_MODEL("array[1..5] of var 0..1: as;\n"
              "constraint 0 = sum(i in 1..5)(as[i+1] = 1);");
    LZN_EXPECTED();
  }

  SECTION("simple inside let") {
    LZN_MODEL("var 0..1: x;\n"
              "constraint let {var 0..1: y} in y = 1 -> x = 1;");
    LZN_EXPECTED(LZN_ONELINE(2, 33, 46));
  }

  LZN_TEST_CASE_END
}
