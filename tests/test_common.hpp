#pragma once

#include <catch2/catch.hpp>
#include <linter/registry.hpp>
#include <minizinc/parser.hh>
#include <minizinc/typecheck.hh>
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

// TODO: statically specify includePaths from CMAKE
#define LZN_MODEL_INIT                                                                             \
  std::vector<std::string> includePaths = {"../../deps/libminizinc/share/minizinc/std/"};          \
  std::stringstream errstream;                                                                     \
  MiniZinc::Env env;

// TODO: gc?
#define LZN_TEST_CASE_INIT(rule_id)                                                                \
  const LZN::LintRule *rule;                                                                       \
  REQUIRE_NOTHROW(rule = LZN::Registry::get(rule_id));                                             \
  std::vector<LZN::LintResult> results;                                                            \
  LZN_MODEL_INIT

#define LZN_TEST_CASE_END                                                                          \
  UNSCOPED_INFO("MiniZinc::parse printed some error");                                             \
  char buf;                                                                                        \
  REQUIRE(errstream.readsome(&buf, 1) == 0);

#define LZN_ONLY_PARSE(s)                                                                          \
  MiniZinc::Model *model = MiniZinc::parse(env, {}, {}, (s), MODEL_FILENAME, includePaths, false,  \
                                           false, false, false, errstream);                        \
  if (model == nullptr)                                                                            \
    errstream >> std::cerr.rdbuf();                                                                \
  assert(model != nullptr);                                                                        \
  std::vector<MiniZinc::TypeError> typeErrors;                                                     \
  MiniZinc::typecheck(env, model, typeErrors, true, false);                                        \
  LZN::LintEnv lenv(model, env, includePaths);

#define LZN_MODEL(s)                                                                               \
  LZN_ONLY_PARSE(s);                                                                               \
  rule->run(lenv);                                                                                 \
  results = lenv.take_results();

#define LZN_ONELINE(...)                                                                           \
  LZN::LintResult(MODEL_FILENAME, rule, "", LZN::FileContents::OneLineMarked{__VA_ARGS__})
