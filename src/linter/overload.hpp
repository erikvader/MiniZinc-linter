#pragma once

// handy thing to provide a bunch of lambdas to std::visit
// https://en.cppreference.com/w/cpp/utility/variant/visit
// https://www.bfilipek.com/2019/02/2lines3featuresoverload.html
template <class... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overload(Ts...) -> overload<Ts...>;
