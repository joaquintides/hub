/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/hub.hpp>
#include <functional>
#include <memory>
#include <vector>
#include "utility.hpp"

struct tidy_int
{
  tidy_int(int n_ = 0): n{n_} {}
  ~tidy_int() { n = 0xDEADBEEF; }

  operator int() const { return n; }

  tidy_int& operator+=(const tidy_int& x)
  {
    n += x.n;
    return *this;
  }

  int n;
};

template<typename Iterator>
using erase_callback = std::function<void(Iterator)>;

template<typename Iterator>
struct track_info
{
  Iterator                      it;
  typename Iterator::pointer    pointer;
  typename Iterator::value_type value;

  bool valid() const
  {
    return std::addressof(*it) == pointer && *it == value;
  }
};

template<typename Hub, typename F>
void test_stability(Hub& x, F f)
{
  using value_type = typename Hub::value_type;
  using iterator = typename Hub::iterator;
  using track_info = ::track_info<iterator>;
  using track_vector = std::vector<track_info>;

  auto last = x.end();
  track_vector track;
  for(auto it = x.begin(); it != last; ++it) {
    track.push_back(track_info{it, std::addressof(*it), *it});
  }
  f(erase_callback<iterator>{[&] (iterator it) {
    track.erase(std::find_if(
      track.begin(), track.end(),
      [&] (const track_info& info) {  return info.it == it; }));
  }});
  BOOST_TEST(x.end() == last);
  for(const auto& info: track) BOOST_TEST(info.valid());
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
  {
    Hub x(rng.begin(), rng.end());
    puncture(x);
    test_stability(x, [&] (erase_callback callback) {
      x.insert(rng.begin(), rng.end());
    });
  }
  {
    Hub x(rng.begin(), rng.begin() + rng.size() / 2),
        y(rng.begin() + rng.size() / 2, rng.end());
    test_stability(x, [&] (erase_callback) {
      test_stability(y, [&] (erase_callback) {
        x.swap(y);
      });
    });
  }
}

int main()
{
  test<boost::hub<int>>();
  test<boost::hub<tidy_int>>();

  return boost::report_errors();
}
