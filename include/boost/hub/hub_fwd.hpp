/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HUB_HUB_FWD_HPP
#define BOOST_HUB_HUB_FWD_HPP

#include <memory>

#ifndef BOOST_NO_CXX17_HDR_MEMORY_RESOURCE
#include <memory_resource>
#endif

namespace boost {

namespace hubs {

template<typename T, typename Allocator = std::allocator<T>>
class hub;

template<typename T, typename Allocator>
void swap(hub<T, Allocator>& x, hub<T, Allocator>& y)
  noexcept(noexcept(x.swap(y)));

template<typename T, typename Allocator, typename Predicate>
typename hub<T, Allocator>::size_type erase_if(hub<T, Allocator>&, Predicate);

template<typename T, typename Allocator, typename U = T>
typename hub<T, Allocator>::size_type erase(hub<T, Allocator>&, const U&);

#ifndef BOOST_NO_CXX17_HDR_MEMORY_RESOURCE
namespace pmr {

template<typename T>
using hub = boost::hubs::hub<T, std::pmr::polymorphic_allocator<T>>;

}
#endif

} /* namespace hubs */

using hubs::hub;

} /* namespace boost */

#endif
