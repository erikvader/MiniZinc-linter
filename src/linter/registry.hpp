#pragma once

#include <linter/rules.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace LZN {

// Stores all LintRules. They are added upon initialization, before main. The rules can also be
// iterated.
class Registry {
private:
  using RuleMap = std::unordered_map<lintId, const LintRule *>;

  // Get the map where all rules are stores. Helps with static initialization order fiasco.
  static RuleMap &getMap() noexcept;

public:
  static bool add(const LintRule *i);
  static const LintRule *get(lintId id);
  static std::size_t size() noexcept;

  // Iterator over all rules
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

  // Makes it possible to use: for(auto rule | Registry::iter()) {}
  class Iter {
  public:
    iterator begin() const noexcept;
    iterator end() const noexcept;
  };

  // Returns an object that can be iterated over.
  static Iter iter() noexcept;
};

} // namespace LZN

// A macro each rule uses to add themselves to the registry.
#define REGISTER_RULE(r)                                                                           \
  namespace {                                                                                      \
  using namespace LZN;                                                                             \
  constexpr r r##_VALUE;                                                                           \
  bool r##_BOOL = Registry::add(&r##_VALUE);                                                       \
  }
