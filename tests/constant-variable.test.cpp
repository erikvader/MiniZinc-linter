#include "test_common.hpp"

TEST_CASE("variable assigned to par", "[rule]") {
  LZN_TEST_CASE_INIT(4);

  SECTION("one variable not assigned") {
    LZN_MODEL("var int: x;");
    LZN_EXPECTED();
  }

  SECTION("one parameter not assigned") {
    LZN_MODEL("int: x;");
    LZN_EXPECTED();
  }

  SECTION("one parameter assigned") {
    LZN_MODEL("int: x = 3;");
    LZN_EXPECTED();
  }

  SECTION("one variable assigned to par") {
    LZN_MODEL("var int: x = 4;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
  }

  SECTION("one variable constrained to par") {
    LZN_MODEL("var int: x;\n"
              "constraint x = 2;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
  }

  SECTION("one variable constrained to par nested") {
    LZN_MODEL("var int: x;\n"
              "constraint if 1 = 1 then x = 2 endif;");
    LZN_EXPECTED();
  }

  SECTION("one variable assigned out of place") {
    LZN_MODEL("var int: x;\n"
              "x = 2;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
  }

  SECTION("one variable assigned variable") {
    LZN_MODEL("var int: x;\n"
              "var int: y = x;");
    LZN_EXPECTED();
  }

  SECTION("array assigned to par") {
    LZN_MODEL("array[1..3] of var int: arr = [1,2,3];");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 27));
  }

  SECTION("array constrained equal in forall to par") {
    LZN_MODEL("array[1..3] of var int: arr;\n"
              "constraint forall(i in 1..3)(arr[i] = i*2);");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 27));
  }

  SECTION("array constrained in forall") {
    LZN_MODEL("array[1..3] of var int: arr;\n"
              "constraint forall(i in 1..3)(arr[i] > 0);");
    LZN_EXPECTED();
  }

  SECTION("array constrained equal in forall to var") {
    LZN_MODEL("array[1..3] of var int: arr;\n"
              "constraint forall(i in 1..3)(arr[i] = arr[i]);");
    LZN_EXPECTED();
  }

  SECTION("inside let") {
    LZN_MODEL("var int: x = let {var int: y; constraint y = 3;} in y;");
    LZN_EXPECTED(LZN_ONELINE(1, 19, 28));
  }

  LZN_TEST_CASE_END
}
