/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HUB_TEST_UTILITY_HPP
#define BOOST_HUB_TEST_UTILITY_HPP

#include <cstddef>
#include <vector>

template<typename T>
std::vector<T> make_range(std::size_t n)
{
  std::vector<T> res;
  T i = T();
  while(n--) {
    res.push_back(i);
    i += T(1);
  }
  return res;
}

struct null_callback
{
  template<typename T>
  void operator()(const T&) const {}
};

template<typename Container, typename EraseCallback = null_callback>
void puncture(Container& x, EraseCallback callback = EraseCallback())
{
  for(auto first = x.begin(); first != x.end(); ) {
    if(!(*first % 7)) {
      callback(first);
      first = x.erase(first);
    }
    else ++first;
  }
}

#endif
