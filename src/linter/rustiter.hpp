#pragma once

#include <iterator>
#include <optional>

// Adapt something with a rust-like iterator interface to a C++ iterator.
template <typename RIter, typename Item = typename RIter::Item>
class RustIterator {
  using iterTemplate = std::iterator<std::input_iterator_tag, Item>;
  RIter rIter;

  class iterator : public iterTemplate {
    std::optional<RIter *> rIter;
    std::optional<Item> latest;

  public:
    iterator() = default;
    iterator(RIter &rIter) : rIter(&rIter) { ++(*this); }

    bool operator==(const iterator &) const noexcept { return !latest; }
    bool operator!=(const iterator &other) const noexcept { return !(*this == other); }
    iterator &operator++() {
      if (rIter)
        latest = (*rIter)->next();
      return *this;
    }
    iterator operator++(int) {
      iterator ret = *this;
      ++(*this);
      return ret;
    }
    typename iterTemplate::reference operator*() { return latest.value(); }
    Item take() {
      Item i = std::move(latest.value());
      latest.reset();
      return i;
    }
  };

public:
  RustIterator(RIter r) : rIter(std::move(r)) {}
  template <typename... Args>
  RustIterator(Args &&...args) : rIter(std::forward<Args>(args)...) {}

  iterator begin() { return iterator(rIter); }
  iterator end() { return iterator(); }
};
