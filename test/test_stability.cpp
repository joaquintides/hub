/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/hub.hpp>
#include <functional>
#include <vector>
#include "utility.hpp"

template<typename Iterator>
using erase_callback = std::function<void(Iterator)>;

template<typename Hub, typename F>
void test_stability(Hub& x, F f)
{
  using value_type = typename Hub::value_type;
  using iterator = typename Hub::iterator;
  using track_vector = std::vector<std::pair<iterator, value_type>>;
  using track_vector_value_type = typename track_vector::value_type;

  auto last = x.end();
  track_vector track;
  for(auto it = x.begin(); it != last; ++it) track.emplace_back(it, *it);
  f(erase_callback<iterator>{[&] (iterator it) {
    track.erase(std::find_if(
      track.begin(), track.end(),
      [&] (const track_vector_value_type& v) {  return v.first == it; }));
  }});
  BOOST_TEST(x.end() == last);
  for(const auto& p: track) BOOST_TEST(*p.first == p.second);
}

template<typename Hub>
void test()
{
  using value_type = typename Hub::value_type;
  using iterator = typename Hub::iterator;
  using erase_callback = ::erase_callback<iterator>;

  auto rng = make_range<value_type>(200);

  {
    Hub x(rng.begin(), rng.end());
    test_stability(x, [&] (erase_callback callback) {
      puncture(x, callback);
    });
  }
}

int main()
{
  test<boost::hub<int>>();
  test<boost::hub<std::size_t>>();

  return boost::report_errors();
}
