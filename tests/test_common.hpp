#pragma once

#include <catch2/catch.hpp>
#include <linter/registry.hpp>
#include <minizinc/parser.hh>
#include <vector>

inline constexpr const char *const MODEL_FILENAME = "testmodel";

#define LZN_EXPECTED(...)                                                                          \
  {                                                                                                \
    std::vector<LZN::LintResult> expected = {__VA_ARGS__};                                         \
    REQUIRE(results.size() == expected.size());                                                    \
    std::sort(results.begin(), results.end());                                                     \
    std::sort(expected.begin(), expected.end());                                                   \
    auto results_iter = results.begin();                                                           \
    auto expected_iter = expected.begin();                                                         \
    for (; results_iter != results.end() && expected_iter != expected.end();                       \
         results_iter++, expected_iter++) {                                                        \
      CHECK(*results_iter == *expected_iter);                                                      \
    }                                                                                              \
  }

// TODO: gc?
#define LZN_TEST_CASE_INIT(rule_id)                                                                \
  const LZN::LintRule *rule;                                                                       \
  REQUIRE_NOTHROW(rule = LZN::Registry::get(rule_id));                                             \
  std::stringstream errstream;                                                                     \
  MiniZinc::Env env;                                                                               \
  std::vector<LZN::LintResult> results;

#define LZN_TEST_CASE_END                                                                          \
  UNSCOPED_INFO("MiniZinc::parse printed some error");                                             \
  char buf;                                                                                        \
  REQUIRE(errstream.readsome(&buf, 1) == 0);

#define LZN_MODEL(s)                                                                               \
  rule->run(                                                                                       \
      MiniZinc::parse(env, {}, {}, (s), MODEL_FILENAME, {}, false, true, false, false, errstream), \
      results)

#define LZN_ONELINE(...)                                                                           \
  LZN::LintResult(MODEL_FILENAME, rule, "", LZN::LintResult::OneLineMarked{__VA_ARGS__})
