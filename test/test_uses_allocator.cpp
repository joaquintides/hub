/* Copyright 2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/container/hub.hpp>
#include <boost/core/lightweight_test.hpp>
#include <scoped_allocator>
#include <string>
#include "utility.hpp"

int main()
{
  using string = std::basic_string<
    char, std::char_traits<char>, stateful_allocator<char>>;
  using hub = boost::container::hub<
    string, std::scoped_allocator_adaptor<stateful_allocator<string>>>;
  using allocator_type = hub::allocator_type;

  hub h(allocator_type(42));
  h.emplace("hello");
  BOOST_TEST(*h.begin() == "hello");
  BOOST_TEST_EQ(h.begin()->get_allocator().state, 42);

  return boost::report_errors();
}
