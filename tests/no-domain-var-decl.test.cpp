#include "test_common.hpp"

LZN_TEST_CASE("no domain on variable declarations", "[rule]", 13)

SECTION("one variable") {
  LZN_MODEL("var int: x;");
  LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
}

SECTION("two variables") {
  LZN_MODEL("var float: x1;\n"
            "var int: x2;");
  LZN_EXPECTED(LZN_ONELINE(1, 1, 13), LZN_ONELINE(2, 1, 11));
}

SECTION("one parameter") {
  LZN_MODEL("int: x;");
  LZN_EXPECTED();
}

SECTION("bounded variable") {
  LZN_MODEL("var 1..3: x;");
  LZN_EXPECTED();
}

SECTION("assigned variable") {
  LZN_MODEL("var int: x = 2;");
  LZN_EXPECTED();
}

SECTION("assigned variable out of line") {
  LZN_MODEL("var int: x;\n"
            "x = 5;");
  LZN_EXPECTED();
}

SECTION("assigned parameters") {
  LZN_MODEL("int: zz = 5;"
            "int: zzz;"
            "zzz = 3;");
  LZN_EXPECTED();
}

SECTION("array of unbounded var") {
  LZN_MODEL("array[1..5,1..3] of var int: varar");
  LZN_EXPECTED(LZN_ONELINE(1, 1, 34));
}

SECTION("okay arrays") {
  LZN_MODEL("array[1..5] of var -1..1: varar2;"
            "array[1..5] of int: parar;");
  LZN_EXPECTED();
}

SECTION("inside predicate") {
  LZN_MODEL("predicate asd() = let {\n"
            "    var float : g ::bounds;\n"
            "} in g mod 2 = 0;\n");
  LZN_EXPECTED(LZN_ONELINE(2, 5, 17));
}

LZN_END
