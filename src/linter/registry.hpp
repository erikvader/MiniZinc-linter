#pragma once

#include <linter/rules.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace LZN {

class Registry {
private:
  typedef std::unordered_map<lintId, const LintRule *> RuleMap;

  static RuleMap &getMap() noexcept;

public:
  static bool add(const LintRule *i);
  static const LintRule *get(lintId id);
  static std::size_t size() noexcept;

  class iterator
      : public std::iterator<std::forward_iterator_tag, const RuleMap::value_type::second_type> {
    RuleMap::const_iterator it;

  public:
    explicit iterator(RuleMap::const_iterator &&it) : it(it) {}
    iterator &operator++();
    iterator operator++(int);
    bool operator==(iterator other) const;
    bool operator!=(iterator other) const;
    reference operator*() const;
  };

  class Iter {
  public:
    iterator begin() const noexcept;
    iterator end() const noexcept;
  };

  static Iter iter() noexcept;
};

} // namespace LZN

#define REGISTER_RULE(r)                                                                           \
  namespace {                                                                                      \
  using namespace LZN;                                                                             \
  constexpr r r##_VALUE;                                                                           \
  bool r##_BOOL = Registry::add(&r##_VALUE);                                                       \
  }
