/* Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HUB_TEST_UTILITY_HPP
#define BOOST_HUB_TEST_UTILITY_HPP

#include <cstddef>
#include <memory>
#include <vector>
#include <type_traits>

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

template<
  typename T, 
  typename Propagate = std::false_type, typename AlwaysEqual = std::false_type
>
struct stateful_allocator
{
  using value_type = T;
  using propagate_on_container_copy_assignment = Propagate;
  using propagate_on_container_move_assignment = Propagate;
  using propagate_on_container_swap = Propagate;
  using is_always_equal = AlwaysEqual;

  stateful_allocator(int state_ = 0): state{state_} {}

  template<typename U>
  stateful_allocator(const stateful_allocator<U,Propagate,AlwaysEqual>& x):
    state{x.state}, num_allocations{x.num_allocations} {}

  T* allocate(std::size_t n)
  {
    auto p = static_cast<T*>(::operator new(n * sizeof(T)));
    ++num_allocations;
    return p;
  }

  void deallocate(T* p, std::size_t) { ::operator delete(p); }

  bool operator==(const stateful_allocator& x) const
  {
    return AlwaysEqual::value || (state == x.state);
  }

  bool operator!=(const stateful_allocator& x) const { return !(*this == x); }

  int state;
  int num_allocations = 0;
};

#endif
