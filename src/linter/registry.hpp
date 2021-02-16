#pragma once

#include <linter/rules.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace LZN {

// TODO: custom iterator

class Registry {
private:
  typedef std::unordered_map<lintId, const LintRule *> RuleMap;

  static RuleMap &getMap() noexcept;

public:
  static bool add(const LintRule *i);
  static const LintRule *get(lintId id);
  static RuleMap::const_iterator begin() noexcept;
  static RuleMap::const_iterator end() noexcept;
  static std::size_t size() noexcept;
};

} // namespace LZN

#define REGISTER_RULE(r)                                                                           \
  namespace {                                                                                      \
  using namespace LZN;                                                                             \
  constexpr r r##_VALUE;                                                                           \
  bool r##_BOOL = Registry::add(&r##_VALUE);                                                       \
  }
