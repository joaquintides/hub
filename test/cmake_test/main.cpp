/* Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/container/hub.hpp>

int main()
{
  boost::container::hub<int> x;
  x.insert(0);

  return 0;
}