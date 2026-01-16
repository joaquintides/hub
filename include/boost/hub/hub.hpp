/* Hub container.
 * 
 * Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HUB_HUB_HPP
#define BOOST_HUB_HUB_HPP

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/bit.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/core/pointer_traits.hpp>
#include <boost/hub/hub_fwd.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#if defined(BOOST_NO_CXX20_HDR_CONCEPTS) || defined(BOOST_NO_CXX20_HDR_RANGES)
#define BOOST_HUB_NO_RANGES
#elif BOOST_WORKAROUND(BOOST_CLANG_VERSION, < 170100) && \
      defined(BOOST_LIBSTDCXX_VERSION)
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109647
 * https://github.com/llvm/llvm-project/issues/49620
 */
#define BOOST_HUB_NO_RANGES
#endif

#if !defined(BOOST_HUB_NO_RANGES)
#include <concepts>
#include <ranges>
#endif

#if !defined(BOOST_HUB_DISABLE_SSE2)
#if defined(BOOST_HUB_ENABLE_SSE2)|| \
    defined(__SSE2__)|| \
    defined(_M_X64)||(defined(_M_IX86_FP)&&_M_IX86_FP>=2)
#define BOOST_HUB_SSE2
#endif
#endif

#if defined(BOOST_HUB_SSE2)
#include <emmintrin.h>
#endif

#ifdef __has_builtin
#define BOOST_HUB_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BOOST_HUB_HAS_BUILTIN(x) 0
#endif

#if !defined(NDEBUG)
#define BOOST_HUB_ASSUME(cond) BOOST_ASSERT(cond)
#elif BOOST_HUB_HAS_BUILTIN(__builtin_assume)
#define BOOST_HUB_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__) || BOOST_HUB_HAS_BUILTIN(__builtin_unreachable)
#define BOOST_HUB_ASSUME(cond)           \
  do{                                    \
    if(!(cond)) __builtin_unreachable(); \
  } while(0)
#elif defined(_MSC_VER)
#define BOOST_HUB_ASSUME(cond) __assume(cond)
#else
#define BOOST_HUB_ASSUME(cond)          \
  do{                                   \
    static_cast<void>(false && (cond)); \
  } while(0)
#endif

/* We use BOOST_HUB_PREFETCH[_BLOCK] macros rather than proper
 * functions because of https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109985
 */

#if defined(BOOST_GCC) || defined(BOOST_CLANG)
#define BOOST_HUB_PREFETCH(p) \
__builtin_prefetch((const char*)boost::to_address(p))
#elif defined(BOOST_HUB_SSE2)
#define BOOST_HUB_PREFETCH(p) \
_mm_prefetch((const char*)boost::to_address(p), _MM_HINT_T0)
#else
#define BOOST_HUB_PREFETCH(p) ((void)(p))
#endif

#define BOOST_HUB_PREFETCH_BLOCK(pbb, Block) \
do{                                          \
  auto p0 = &static_cast<Block&>(*(pbb));    \
  BOOST_HUB_PREFETCH(p0->data);              \
} while(0)

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

namespace boost {

namespace hubs {

namespace detail {

inline int unchecked_countr_zero(std::uint64_t x)
{
#if defined(BOOST_MSVC) && (defined(_M_X64) || defined(_M_ARM64))
  unsigned long r;
  _BitScanForward64(&r,x);
  return (int)r;
#elif defined(BOOST_GCC) || defined(BOOST_CLANG)
  return (int)__builtin_ctzll(x);
#else
  BOOST_HUB_ASSUME(x != 0);
  return (int)core::countr_zero(x);
#endif
}

inline int unchecked_countr_one(std::uint64_t x)
{
  return unchecked_countr_zero(~x);
}

inline int unchecked_countl_zero(std::uint64_t x)
{
#if defined(BOOST_MSVC) && (defined(_M_X64) || defined(_M_ARM64))
  unsigned long r;
  _BitScanReverse64(&r,x);
  return (int)(63 - r);
#elif defined(BOOST_GCC) || defined(BOOST_CLANG)
  return (int)__builtin_clzll(x);
#else  
  BOOST_HUB_ASSUME(x != 0);
  return (int)core::countl_zero(x);
#endif
}

template<typename Pointer, typename T>
using pointer_rebind_t = 
  typename pointer_traits<Pointer>::template rebind<T>;

template<typename VoidPointer>
struct block_base
{
  using pointer = pointer_rebind_t<VoidPointer, block_base>;
  using const_pointer = pointer_rebind_t<VoidPointer, const block_base>;
  using mask_type = std::uint64_t;

  static constexpr int N = 64;
  static constexpr mask_type full = (mask_type)(-1);

  static pointer pointer_to(block_base& x) noexcept
  {
    return pointer_traits<pointer>::pointer_to(x);
  }

  static const_pointer pointer_to(const block_base& x) noexcept
  {
    return pointer_traits<const_pointer>::pointer_to(x);
  }

#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
  BOOST_FORCEINLINE void link_available_before(pointer p) noexcept
  {
    next_available = p;
    prev_available = p->prev_available;
    next_available->prev_available = pointer_to(*this);
    prev_available->next_available = pointer_to(*this);
  }

  BOOST_FORCEINLINE void link_available_after(pointer p) noexcept
  {
    prev_available = p;
    next_available = p->next_available;
    next_available->prev_available = pointer_to(*this);
    prev_available->next_available = pointer_to(*this);
  }

  BOOST_FORCEINLINE void unlink_available() noexcept
  {
    prev_available->next_available = next_available;
    next_available->prev_available = prev_available;
  }
#else
  BOOST_FORCEINLINE void link_available_after(pointer p) noexcept
  {
    next_available = p->next_available;
    p->next_available = pointer_to(*this);
  }

  BOOST_FORCEINLINE void unlink_available_after(pointer p) noexcept
  {
    p->next_available = next_available;
  }
#endif

  BOOST_FORCEINLINE void link_before(pointer p) noexcept
  {
    next = p;
    prev = p->prev;
    next->prev = pointer_to(*this);
    prev->next = pointer_to(*this);
  }

  BOOST_FORCEINLINE void unlink() noexcept
  {
    prev->next = next;
    next->prev = prev;
  }

#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
  pointer   prev_available,
            next_available,
#else
  pointer   next_available,
#endif
            prev,
            next;
  mask_type mask;
};

template<typename ValuePointer>
struct block: block_base<pointer_rebind_t<ValuePointer, void>>
{
  using super = block_base<pointer_rebind_t<ValuePointer, void>>;
  ValuePointer data;
};

template<typename ValuePointer>
void swap_payload(block<ValuePointer>& x, block<ValuePointer>& y) noexcept
{
  std::swap(x.mask, y.mask);
  std::swap(x.data, y.data);
}

template<typename ValuePointer>
struct block_list: block<ValuePointer>
{
  using block = detail::block<ValuePointer>;
  using block_base = typename block::super;
  using block_base_pointer = typename block_base::pointer;
  using const_block_base_pointer = typename block_base::const_pointer;
  using block_pointer = pointer_rebind_t<ValuePointer, block>;
  using block::full;
  using block::pointer_to;
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
  using block::prev_available;
#endif
  using block::next_available;
  using block::prev;
  using block::next;
  using block::mask;
  using block::data;

  static block_pointer 
  static_cast_block_pointer(block_base_pointer pbb) noexcept
  {
    return pointer_traits<block_pointer>::pointer_to(
      static_cast<block&>(*pbb));
  }

  block_list() 
  { 
    reset();
    mask = 1; /* sentinel */
    data = nullptr;
  }

  block_list(block_list&& x) noexcept: block_list{}
  {
    if(x.next_available != x.header()) {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
      prev_available = x.prev_available;
      next_available = x.next_available;
      next_available->prev_available = header();
      prev_available->next_available = header();
#else
      next_available = x.next_available;
      last_available = x.last_available;
      last_available->next_available = header();
#endif
    }
    if(x.prev != x.header()) {
      prev = x.prev;
      next = x.next;
      next->prev = header();
      prev->next = header();
    }
    x.reset();
  }

  block_list& operator=(block_list&& x) noexcept
  {
    reset();
    if(x.next_available != x.header()) {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
      prev_available = x.prev_available;
      next_available = x.next_available;
      next_available->prev_available = header();
      prev_available->next_available = header();
#else
      next_available = x.next_available;
      last_available = x.last_available;
      last_available->next_available = header();
#endif
    }
    if(x.prev != x.header()) {
      prev = x.prev;
      next = x.next;
      next->prev = header();
      prev->next = header();
    }
    x.reset();
    return *this;
  }

  void reset() noexcept
  {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
    prev_available = header();
    next_available = header();
    prev = header();
    next = header();
#else
    next_available = header();
    prev = header();
    next = header();
    last_available = header();
#endif
  }

  block_base_pointer header() noexcept 
  {
    return pointer_to(static_cast<block_base&>(*this)); 
  }

  const_block_base_pointer header() const noexcept 
  {
    return pointer_to(static_cast<const block_base&>(*this)); 
  }

  BOOST_FORCEINLINE void link_at_back(block_pointer pb) noexcept 
  {
    pb->link_before(header());
  }

  BOOST_FORCEINLINE static void unlink(block_pointer pb) noexcept
  {
    pb->unlink();
  }

  BOOST_FORCEINLINE void link_available_at_back(block_pointer pb) noexcept 
  {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
    pb->link_available_before(header());
#else
    pb->link_available_after(last_available);
    last_available = pb;
#endif
  }

  BOOST_FORCEINLINE void link_available_at_front(block_pointer pb) noexcept 
  {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
    pb->link_available_after(header());
#else
    if(last_available == header()) last_available = pb;
    pb->link_available_after(header());
#endif
  }

  BOOST_FORCEINLINE void unlink_available(block_pointer pb) noexcept
  {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
    pb->unlink_available();
    BOOST_HUB_PREFETCH(next_available);
#else
    BOOST_ASSERT(next_available == pb);
    pb->unlink_available_after(header());
    if(last_available == pb) last_available = header();
    BOOST_HUB_PREFETCH(next_available);
#endif
  }

  BOOST_FORCEINLINE void unlink_available_after(
    block_pointer pb, block_base_pointer pbb_prev) noexcept
  {
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
    (void)pbb_prev;
    unlink_available(pb);
#else
    pb->unlink_available_after(pbb_prev);
    if(last_available == pb) last_available = header();
#endif
  }

  void purge_unavailable() noexcept
  {
    for(auto pbb_prev = header(), pbb = pbb_prev->next_available;
        pbb != header(); ) {
      auto pb = static_cast_block_pointer(pbb);
      pbb = pbb->next_available;
      if(pb->mask == full) unlink_available_after(pb, pbb_prev);
      else pbb_prev = pb;
    }
  }

#if !defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
  block_base_pointer last_available;
#endif
};

template<typename ValuePointer>
class iterator
{
  using element_type = typename pointer_traits<ValuePointer>::element_type;
  template<typename Value2Pointer>
  using enable_if_consts_to_element_type_t =typename std::enable_if<
    std::is_same<
      const typename pointer_traits<Value2Pointer>::element_type, 
      element_type>::value
  >::type;

public:
  using value_type = typename std::remove_const<element_type>::type;
  using difference_type = 
    typename pointer_traits<ValuePointer>::difference_type;
  using pointer = ValuePointer;
  using reference = element_type&;
  using iterator_category = std::bidirectional_iterator_tag;

  iterator() = default;
  iterator(const iterator&) = default;

  template<
    typename Value2Pointer,
    typename = enable_if_consts_to_element_type_t<Value2Pointer>
  >
  iterator(const iterator<Value2Pointer>& x) noexcept: pbb{x.pbb}, n{x.n} {}
      
  iterator& operator=(const iterator& x) = default;

  template<
    typename Value2Pointer,
    typename = enable_if_consts_to_element_type_t<Value2Pointer>
  >
  iterator& operator=(const iterator<Value2Pointer>& x) noexcept
  {
    pbb = x.pbb;
    n = x.n;
    return *this;
  }

  pointer operator->() const noexcept
  {
    return static_cast<block&>(*pbb).data + n;
  }

  reference operator*() const noexcept
  {
    return *operator->();
  }

  BOOST_FORCEINLINE iterator& operator++() noexcept
  {
    auto mask = (pbb->mask >> n) - 1;
    if(BOOST_LIKELY(mask != 0)) {
      n += detail::unchecked_countr_zero(mask);
    }
    else {
      pbb = pbb->next;
      BOOST_HUB_PREFETCH_BLOCK(pbb->next, block);
      n = detail::unchecked_countr_zero(pbb->mask);
    }
    return *this;
  }

  BOOST_FORCEINLINE iterator operator++(int) noexcept
  {
    iterator tmp(*this);
    this->operator++();
    return tmp;
  }

  BOOST_FORCEINLINE iterator& operator--() noexcept
  {
    auto mask = (pbb->mask << (N - 1 - n)) - ((mask_type)1 << (N - 1));
    if(BOOST_LIKELY(mask != 0)) {
      n -= detail::unchecked_countl_zero(mask);
    }
    else {
      pbb = pbb->prev;
      BOOST_HUB_PREFETCH_BLOCK(pbb->prev, block);
      n = N - 1 - detail::unchecked_countl_zero(pbb->mask);
    }
    return *this;
  }

  BOOST_FORCEINLINE iterator operator--(int) noexcept
  {
    iterator tmp(*this);
    this->operator--();
    return tmp;
  }

  friend bool operator==(const iterator& x, const iterator& y) noexcept
  {
    return x.pbb == y.pbb && x.n == y.n;
  }
  
  friend bool operator!=(const iterator& x, const iterator& y) noexcept
  {
    return !(x == y);
  }

private:
  template<typename> friend class iterator;
  template<typename, typename> friend class hubs::hub;

  template<typename T>
  using pointer_rebind_t = detail::pointer_rebind_t<ValuePointer, T>;
  using block_base = detail::block_base<pointer_rebind_t<void>>;
  using block_base_pointer = pointer_rebind_t<block_base>;
  using const_block_base_pointer = pointer_rebind_t<const block_base>;
  using block = detail::block<pointer_rebind_t<value_type>>;
  using mask_type = typename block_base::mask_type;

  static constexpr int N = block_base::N;

  iterator(const_block_base_pointer pbb_, int n_) noexcept:
    pbb{const_cast_block_base_pointer(pbb_)}, n{n_} {}

  iterator(const_block_base_pointer pbb_) noexcept:
    pbb{const_cast_block_base_pointer(pbb_)}, 
    n{detail::unchecked_countr_zero(pbb->mask)} 
  {}

  static block_base_pointer
  const_cast_block_base_pointer(const_block_base_pointer pbb_) noexcept
  {
    return block_base::pointer_to(const_cast<block_base&>(*pbb_));
  }

  block_base_pointer pbb = nullptr;
  int                n = 0;
};

template<typename T, std::size_t N>
struct sort_iterator
{
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;

  sort_iterator(T** pp_, std::size_t index_): pp{pp_}, index{index_} {}

  pointer operator->() const noexcept
  {
    return pp[index / N] + (index % N);
  }

  reference operator*() const noexcept
  {
    return *(operator->());
  }

  sort_iterator& operator++() noexcept
  {
    ++index;
    return *this;
  }

  sort_iterator operator++(int) noexcept
  {
    sort_iterator tmp(*this);
    ++index;
    return tmp;
  }

  sort_iterator& operator--() noexcept
  {
    --index;
    return *this;
  }

  sort_iterator operator--(int) noexcept
  {
    sort_iterator tmp(*this);
    --index;
    return tmp;
  }

  friend difference_type
  operator-(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return (difference_type)(x.index - y.index);
  }

  sort_iterator& operator+=(difference_type n) noexcept
  {
    index += n;
    return *this;
  }
    
  friend sort_iterator
  operator+(const sort_iterator& x, difference_type n) noexcept
  {
    return {x.pp, x.index + n};
  }

  friend sort_iterator 
  operator+(difference_type n, const sort_iterator& x) noexcept
  {
    return {x.pp, n + x.index};
  }

  sort_iterator& operator-=(difference_type n) noexcept
  {
    index -= n;
    return *this;
  }
    
  friend sort_iterator 
  operator-(const sort_iterator& x, difference_type n) noexcept
  {
    return {x.pp, x.index - n};
  }

  reference operator[](difference_type n) const noexcept
  {
    return operator*(*this + n);
  }

  friend bool 
  operator==(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return x.index == y.index;
  }
  
  friend bool
  operator!=(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return x.index != y.index;
  }

  friend bool
  operator<(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return x.index < y.index;
  }

  friend bool
  operator>(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return x.index > y.index;
  }

  friend bool
  operator<=(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return x.index <= y.index;
  }

  friend bool
  operator>=(const sort_iterator& x, const sort_iterator& y) noexcept
  {
    return x.index >= y.index;
  }

  T** pp;
  std::size_t index;
};

template<typename T>
struct type_identity { using type = T; };

template<typename T>
using type_identity_t = typename type_identity<T>::type;

#if !defined(BOOST_HUB_NO_RANGES)
template<class R, class T>
concept container_compatible_range =
  std::ranges::input_range<R> &&
  std::convertible_to<std::ranges::range_reference_t<R>, T>;

/* Use own from_range_t only if std::from_range_t does not exist.
 * Technique explained at
 https://bannalia.blogspot.com/2016/09/compile-time-checking-existence-of.html
 */

struct from_range_t{ explicit from_range_t() = default; };
struct from_range_t_hook{};

} /* namespace detail */
} /* namespace hubs */
} /* namespace boost */

namespace std {

template<> struct hash< ::boost::hubs::detail::from_range_t_hook>
{
  using from_range_t_type = decltype([] {
    using namespace ::boost::hubs::detail;
    return from_range_t{};
  }());

  /* make standard happy */
  std::size_t operator()(
    const ::boost::hubs::detail::from_range_t_hook&) const;
};

}

namespace boost {
namespace hubs {

using from_range_t = 
  typename std::hash<detail::from_range_t_hook>::from_range_t_type;
inline constexpr from_range_t from_range {};

namespace detail {
#endif

template<typename InputIterator>
using enable_if_is_input_iterator_t =
  typename std::enable_if<
    std::is_convertible<
      typename std::iterator_traits<InputIterator>::iterator_category,
      std::input_iterator_tag
    >::value
  >::type;

struct if_constexpr_void_else{ void operator()() const {} };

template<typename F, typename G = if_constexpr_void_else>
void if_constexpr(std::true_type, F f, G = G{}) { f(); }

template<typename F, typename G>
void if_constexpr(std::false_type, F, G g) { g(); }

template<typename T>
void copy_assign_if(std::true_type, T& x, const T& y) { x = y; }

template<typename T>
void copy_assign_if(std::false_type, T&, const T&) {}

template<typename T>
void move_assign_if(std::true_type, T& x, T& y) { x = std::move(y); }

template<typename T>
void move_assign_if(std::false_type, T&, T&) {}

template<typename T>
void swap_if(std::true_type, T& x, T& y) { using std::swap; swap(x, y); }

template<typename T>
void swap_if(std::false_type, T&, T&) {}

template<typename Allocator>
struct block_typedefs
{
  using pointer = allocator_pointer_t<Allocator>;
  template<typename Q>
  using pointer_rebind_t = detail::pointer_rebind_t<pointer, Q>;

  using block_base = detail::block_base<pointer_rebind_t<void>>;
  using block_base_pointer = pointer_rebind_t<block_base>;
  using const_block_base_pointer = pointer_rebind_t<const block_base>;
  using block = detail::block<pointer>;
  using block_pointer = pointer_rebind_t<block>;
  using block_allocator = allocator_rebind_t<Allocator,block>;
  using block_list = detail::block_list<pointer>;
};

} /* namespace hubs::detail */

template<typename T, typename Allocator>
class hub: empty_value<
  typename detail::block_typedefs<Allocator>::block_allocator, 0>
{
  static_assert(
    !std::is_const<T>::value && !std::is_volatile<T>::value && 
    !std::is_function<T>::value && !std::is_reference<T>::value && 
    !std::is_void<T>::value,
    "T must be a cv-unqualified object type");
  static_assert(
    std::is_same<T, allocator_value_type_t<Allocator>>::value,
    "Allocator's value_type must be the same type as T");

public:
  using value_type = T;
  using allocator_type = Allocator;
  using pointer = allocator_pointer_t<Allocator>;
  using const_pointer = allocator_const_pointer_t<Allocator>;
  using reference = T&;
  using const_reference = const T&;
  using size_type = allocator_size_type_t<Allocator>;
  using difference_type = allocator_difference_type_t<Allocator>;
  using iterator = detail::iterator<pointer>;
  using const_iterator = detail::iterator<const_pointer>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>; 

  hub() noexcept(noexcept(Allocator())): hub{Allocator()} {}

  explicit hub(const Allocator& al_) noexcept: 
    allocator_base{empty_init, al_} {}

  explicit hub(size_type n, const Allocator& al_ = Allocator()): hub{al_}
  {
    range_insert_impl(size_type(0), n, [&, this] (T* p, size_type) {
      allocator_construct(al(), p);
    });
  }

  hub(size_type n, const T& x, const Allocator& al_ = Allocator()): hub{al_}
  {
    insert(n, x);
  }

  template<
    typename InputIterator, 
    typename = detail::enable_if_is_input_iterator_t<InputIterator>
  >
  hub(
    InputIterator first, InputIterator last,
    const Allocator& al_ = Allocator()): hub{al_}
  {
    insert(first, last);
  }

#if !defined(BOOST_HUB_NO_RANGES)
  template<detail::container_compatible_range<T> R>
  hub(from_range_t, R&& rg, const Allocator& al_ = Allocator()): hub{al_}
  {
    insert_range(std::forward<R>(rg));
  }
#endif

  hub(const hub& x):
    hub{x, allocator_select_on_container_copy_construction(x.al())} {}

  hub(const hub& x, const detail::type_identity_t<Allocator>& al_):
    hub(x.begin(), x.end(), al_) {}

  hub(hub&& x) noexcept:
    hub{std::move(x), Allocator(std::move(x.al())), std::true_type{}} {}

  hub(hub&& x, const detail::type_identity_t<Allocator>& al_):
    hub{std::move(x), al_, allocator_is_always_equal_t<Allocator>{}} {}

  hub(std::initializer_list<T> il, const Allocator& al_ = Allocator()):
    hub{il.begin(), il.end(), al_} {}

  ~hub() { reset(); }

  hub& operator=(const hub& x)
  {
    using pocca =
      allocator_propagate_on_container_copy_assignment_t<Allocator>;

    if(this != &x) {
      if(al() != x.al() && pocca::value) {
        reset();
        detail::copy_assign_if(pocca{}, al(), x.al());
        insert(x.begin(), x.end());
      }
      else{
        detail::copy_assign_if(pocca{}, al(), x.al());
        assign(x.begin(), x.end());
      }
    }
    return *this;
  }

  hub& operator=(hub&& x)
    noexcept(
      allocator_propagate_on_container_move_assignment_t<Allocator>::value ||
      allocator_is_always_equal_t<Allocator>::value)
  {
    if(this != &x) {
      move_assign(
        x, 
        std::integral_constant<
          bool,
          allocator_propagate_on_container_move_assignment_t<Allocator>::
            value ||
          allocator_is_always_equal_t<Allocator>::value>{});
    }
    return *this;
  }

  hub& operator=(std::initializer_list<T> il)
  {
    assign(il);
    return *this;
  }

  template<
    typename InputIterator,
    typename = detail::enable_if_is_input_iterator_t<InputIterator>
  >
  void assign(InputIterator first, InputIterator last)
  {
    range_assign_impl(
      first, last,
      [this] (T* p, InputIterator it) { allocator_construct(al(), p, *it); },
      [] (T* p, InputIterator it) { *p = *it; });
  }

#if !defined(BOOST_HUB_NO_RANGES)
  template<detail::container_compatible_range<T> R>
  void assign_range(R&& rg)
  {
    range_assign_impl(
      std::ranges::begin(rg), std::ranges::end(rg),
      [this] (T* p, auto it) { allocator_construct(al(), p, *it); },
      [] (T* p, auto it) { *p = *it; });
  }
#endif

  void assign(size_type n, const T& x)
  {
    range_assign_impl(
      size_type(0), n,
      [&, this] (T* p, size_type) { allocator_construct(al(), p, x); },
      [&] (T* p, size_type) { *p = x; });
  }

  void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }

  allocator_type get_allocator() const noexcept { return al(); }

  iterator               begin() noexcept { return ++end(); }
  const_iterator         begin() const noexcept { return ++end(); }
  iterator               end() noexcept { return {blist.header(), 0}; }
  const_iterator         end() const noexcept { return {blist.header(), 0}; }
  reverse_iterator       rbegin() noexcept { return reverse_iterator{end()}; }
  const_reverse_iterator rbegin() const noexcept 
                         { return const_reverse_iterator{end()}; }
  reverse_iterator       rend() noexcept { return reverse_iterator{begin()}; }
  const_reverse_iterator rend() const noexcept 
                         { return const_reverse_iterator{begin()}; }
  const_iterator         cbegin() const noexcept { return begin(); }
  const_iterator         cend() const noexcept { return end(); }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  const_reverse_iterator crend() const noexcept { return rend(); }

  bool      empty() const noexcept { return size_ == 0; }
  size_type size() const noexcept { return size_; }
  size_type max_size() const noexcept { return allocator_max_size(al()) * N;}
  size_type capacity() const noexcept { return num_blocks * N; }
  size_type memory() const noexcept { return num_blocks * (sizeof(block) + sizeof(value_type) * N); }

  void reserve(size_type n)
  {
    while(capacity() < n) (void)create_new_available_block();
  }

  void shrink_to_fit()
  {
    compact();
    trim_capacity();
  }

  void trim_capacity() noexcept { trim_capacity(0); }

  void trim_capacity(size_type n) noexcept
  {
    /* Linear on # available blocks, std::hive is linear on # _reserved_
     * blocks.
     */
    for(auto pbb_prev = blist.header(), pbb = pbb_prev->next_available;
        capacity() > n && pbb != blist.header(); ) {
      auto pb = static_cast_block_pointer(pbb);
      pbb = pbb-> next_available;
      if(pb->mask == 0) {
         blist.unlink_available_after(pb, pbb_prev);
         delete_block(pb);
         --num_blocks;
      }
      else {
        pbb_prev = pb;
      }
    }
  }

  template<typename... Args>
  BOOST_FORCEINLINE iterator emplace(Args&&... args)
  {
    int  n;
    auto pb = retrieve_available_block(n);
    allocator_construct(
      al(), boost::to_address(pb->data + n), std::forward<Args>(args)...);
    pb->mask |= pb->mask + 1;
    if(BOOST_UNLIKELY(pb->mask == 1)) blist.link_at_back(pb);
    else if(BOOST_UNLIKELY(pb->mask == full)) blist.unlink_available(pb);
    ++size_;
    return {pb, n};
  }

  template<typename... Args>
  BOOST_FORCEINLINE iterator emplace_hint(const_iterator, Args&&... args)
  {
    return emplace(std::forward<Args>(args)...);
  }

  BOOST_FORCEINLINE iterator insert(const T& x) { return emplace(x); }
  BOOST_FORCEINLINE iterator insert(const_iterator, const T& x)
                             { return emplace(x); }
  BOOST_FORCEINLINE iterator insert(T&& x) { return emplace(std::move(x)); }
  BOOST_FORCEINLINE iterator insert(const_iterator, T&& x) 
                             { return emplace(std::move(x)); }

  void insert(std::initializer_list<T> il) { insert(il.begin(), il.end()); }

#if !defined(BOOST_HUB_NO_RANGES)
  template<detail::container_compatible_range<T> R>
  void insert_range(R&& rg)
  {
    range_insert_impl(
      std::ranges::begin(rg), std::ranges::end(rg),
      [this] (T* p, auto it) { allocator_construct(al(), p, *it); });
  }
#endif

  template<
    typename InputIterator,
    typename = detail::enable_if_is_input_iterator_t<InputIterator>
  >
  void insert(InputIterator first, InputIterator last)
  {
    range_insert_impl(first, last, [this] (T* p, InputIterator it) {
      allocator_construct(al(), p, *it);
    });
  }

  void insert(size_type n, const T& x)
  {
    range_insert_impl(size_type(0), n, [&, this] (T* p, size_type) {
      allocator_construct(al(), p, x);
    });
  }

  BOOST_FORCEINLINE iterator erase(const_iterator pos)
  {
    auto pb = static_cast_block_pointer(pos.pbb);
    auto n = pos.n;
    ++pos;
    allocator_destroy(al(), boost::to_address(pb->data + n));
    auto bit = (mask_type)(1) << n;
    if(BOOST_UNLIKELY(pb->mask == full)) blist.link_available_at_front(pb);
    else if(BOOST_UNLIKELY(pb->mask == bit)) blist.unlink(pb);
    pb->mask &= ~bit;
    --size_;
    return {pos.pbb, pos.n};
  }

  BOOST_FORCEINLINE void erase_void(const_iterator pos)
  {
    auto pb = static_cast_block_pointer(pos.pbb);
    auto n = pos.n;
    allocator_destroy(al(), pb->data + n);
    auto bit = (mask_type)(1) << n;
    if(BOOST_UNLIKELY(pb->mask == full)) blist.link_available_at_front(pb);
    else if(BOOST_UNLIKELY(pb->mask == bit)) blist.unlink(pb);
    pb->mask &= ~bit;
    --size_;
  }

  iterator erase(const_iterator first, const_iterator last)
  {
    for(auto pbb = first.pbb; first != last; ) {
      first = erase(first);
      if(first.pbb != pbb) break;
    }
    auto pbb = first.pbb;
    if(pbb != last.pbb){
      do {
        auto pb = static_cast_block_pointer(pbb);
        pbb = pb->next;
        BOOST_HUB_PREFETCH_BLOCK(pbb, block);
        size_ -= destroy_all_in_nonempty_block(pb);
        blist.unlink(pb);
        if(BOOST_UNLIKELY(pb->mask == full)) blist.link_available_at_front(pb);
        pb->mask = 0;
      } while(pbb != last.pbb);
      first = {pbb};
    }
    while(first != last) first = erase(first);
    return {last.pbb, last.n};
  }

  void swap(hub& x)
    noexcept(
      allocator_propagate_on_container_swap_t<Allocator>::value ||
      allocator_is_always_equal_t<Allocator>::value)
  {
    using pocs = allocator_propagate_on_container_swap_t<Allocator>;

    detail::if_constexpr(pocs{}, [&, this]{
      detail::swap_if(pocs{}, al(), x.al());
    },
    [&, this]{ /* else */
      BOOST_ASSERT(al() == x.al());
      (void)this;
    });
    std::swap(blist, x.blist);
    std::swap(num_blocks, x.num_blocks);
    std::swap(size_, x.size_);
  }

  void clear() noexcept { erase(begin(), end()); }

  void splice(hub& x)
  {
    BOOST_ASSERT(this != &x);
    BOOST_ASSERT(al() == x.al());
    /* non-full blocks */
    for(auto pbb_prev = x.blist.header(), pbb = pbb_prev->next_available;
        pbb != x.blist.header(); ) {
      auto pb = static_cast_block_pointer(pbb);
      pbb = pbb->next_available;
      if(pb->mask != 0) {
        x.blist.unlink_available_after(pb, pbb_prev);
        blist.link_available_at_front(pb);
        x.blist.unlink(pb);
        blist.link_at_back(pb);
        --x.num_blocks;
        ++num_blocks;
        auto s = core::popcount(pb->mask);
        x.size_ -= s;
        size_ += s;
      }
      else {
        pbb_prev = pb;
      }
    }
    /* full blocks remaining */
    for(auto pbb = x.blist.next; pbb != x.blist.header(); ) {
      BOOST_ASSERT(pbb->mask == full);
      auto pb = static_cast_block_pointer(pbb);
      pbb = pbb->next;
      x.blist.unlink(pb);
      blist.link_at_back(pb);
      --x.num_blocks;
      ++num_blocks;
      x.size_ -= N;
      size_ += N;
    }
  }

  void splice(hub&& x) { splice(x); }

  template<typename BinaryPredicate = std::equal_to<T>>
  size_type unique(BinaryPredicate pred = BinaryPredicate())
  {
    auto s = size_;
    for(auto first = cbegin(), last = cend(); first != last; ) {
      auto next = std::next(first);
      first = erase(
        next,
        std::find_if_not(next, last, [&] (const T& x) {
          return pred(x, *first);
        }));
    }
    return (size_type)(s - size_);
  }

  template<typename Compare = std::less<T>>
  void sort(Compare comp = Compare())
  {
    using sort_iterator = detail::sort_iterator<T, N>;

    if(size_ > 1) {
      /* compact elements and build an array of pointers to data chunks */
      compact();

      struct deleter
      {
        using pointer = T**;
        void operator()(pointer p) noexcept { ::operator delete(p); }
      };
      std::size_t n = (std::size_t)((size_ + N - 1) / N);
      std::unique_ptr<T*[], deleter> p
        {static_cast<T**>(::operator new(n * sizeof(T*)))};
      std::size_t i = 0;
      for(auto pbb = blist.next; pbb != blist.header(); pbb = pbb->next) {
        p[i++] = boost::to_address(static_cast_block_pointer(pbb)->data);
      }

      std::sort(
        sort_iterator{p.get(), 0}, sort_iterator{p.get(), size_}, comp);
    }
  }

  iterator get_iterator(const_pointer p) noexcept /* noexcept? */
  {   
    std::less<const T*> less;
    for(auto pbb = blist.next; pbb != blist.header(); pbb = pbb-> next) {
      auto pb = static_cast_block_pointer(pbb);
      if(!less(boost::to_address(p), boost::to_address(pb->data)) &&
          less(boost::to_address(p), boost::to_address(pb->data + N))) {
        return {pb, (int)(p - pb->data)};
      }
    }
    return end(); /* shouldn't assert? */
  }

  const_iterator get_iterator(const_pointer p) const noexcept /* noexcept? */
  {
    return const_cast<hub*>(this)->get_iterator(p);
  }

  template<typename F>
  void visit(iterator first, iterator last, F f)
  {
    visit_while(first, last, [&] (value_type& x) {
      f(x); 
      return true;
    });
  }

  template<typename F>
  void visit(const_iterator first, const_iterator last, F f) const
  {
    visit_while(first, last, [&] (const value_type& x) {
      f(x); 
      return true;
    });
  }

  template<typename F>
  iterator visit_while(iterator first, iterator last, F f)
  {
    for(auto pbb = first.pbb; first != last; ) {
      if(!f(*first)) return first;
      ++first;
      if(first.pbb != pbb) break;
    }
    auto pbb = first.pbb;
    if(pbb != last.pbb){
      do {
        auto pb = static_cast_block_pointer(pbb);
        pbb = pb->next;
        BOOST_HUB_PREFETCH_BLOCK(pbb, block);
        auto mask = pb->mask;
        do {
          auto n = detail::unchecked_countr_zero(mask);
          if(!f(pb->data[n])) return {pbb, n};
          mask &= mask - 1;
        } while(mask);
      } while(pbb != last.pbb);
      first = {pbb};
    }
    for(; first != last; ++first) if(!f(*first)) return first;
    return first;
  }

  template<typename F>
  const_iterator visit_while(
    const_iterator first, const_iterator last, F f) const
  {
    auto it =const_cast<hub*>(this)->visit_while(
      iterator{first.pbb, first.n}, iterator{last.pbb, last.n},
      [&] (const value_type& x) { return f(x); });
    return {it.pbb, it.n};
  }

  template<typename F>
  void visit_all(F f) 
  {
    visit(begin(), end(), std::ref(f)); 
  }

  template<typename F>
  void visit_all(F f) const
  {
    visit(begin(), end(), std::ref(f)); 
  }

  template<typename F>
  iterator visit_all_while(F f) 
  {
    return visit_while(begin(), end(), std::ref(f)); 
  }

  template<typename F>
  const_iterator visit_all_while(F f) const
  {
    return visit_while(begin(), end(), std::ref(f)); 
  }

private:
  using block_typedefs = detail::block_typedefs<Allocator>;
  using block_base = typename block_typedefs::block_base;
  using block_base_pointer = typename block_typedefs::block_base_pointer;
  using const_block_base_pointer = 
    typename block_typedefs::const_block_base_pointer;
  using block = typename block_typedefs::block;
  using block_pointer = typename block_typedefs::block_pointer;
  using block_allocator = typename block_typedefs::block_allocator;
  using block_list = typename block_typedefs::block_list;
  using allocator_base = empty_value<block_allocator, 0>;
  using mask_type = typename block_base::mask_type;

  static constexpr int N = block_base::N;
  static constexpr mask_type full = block_base::full;

  block_allocator&       al() noexcept { return allocator_base::get(); }
  const block_allocator& al() const noexcept { return allocator_base::get(); }

  struct reset_on_exit
  {
    ~reset_on_exit() { x.reset(); }

    hub& x;
  };

  struct purge_unavailable_on_exit
  {
    ~purge_unavailable_on_exit() { x.blist.purge_unavailable(); }

    hub& x;
  };

  hub(
    hub&& x, const Allocator& al_, std::true_type /* equal allocs */) noexcept:
    allocator_base{empty_init, al_}, blist{std::move(x.blist)},
    num_blocks{x.num_blocks}, size_{x.size_}
  {
    x.num_blocks = 0;
    x.size_ = 0;
  }

  hub(
    hub&& x, const Allocator& al_, std::false_type /* maybe unequal allocs */):
    hub{al_}
  {
    if(al() == x.al()) {
      blist = std::move(x.blist);
      num_blocks = x.num_blocks;
      size_ = x.size_;
      x.num_blocks = 0;
      x.size_ = 0;
    }
    else {
      reset_on_exit on_exit{x}; (void)on_exit;
      range_insert_impl(x.begin(), x.end(), [this] (T* p, iterator it) {
        allocator_construct(al(), p, std::move(*it));
      });
    }
  }

  void move_assign(hub& x, std::true_type /* transfer structure */)
  {
    using pocma =
      allocator_propagate_on_container_move_assignment_t<Allocator>;

    reset();
    detail::move_assign_if(pocma{}, al(), x.al());
    blist = std::move(x.blist);
    num_blocks = x.num_blocks;
    size_ = x.size_;
    x.num_blocks = 0;
    x.size_ = 0;
  }

  void move_assign(hub& x, std::false_type /* maybe move data */)
  {
    if(al() == x.al()) {
      move_assign(x, std::true_type{});
    }
    else {
      reset_on_exit on_exit{x}; (void)on_exit;
      range_assign_impl(
        x.begin(), x.end(),
        [this] (T* p, iterator it) 
          { allocator_construct(al(), p, std::move(*it)); },
        [] (T* p, iterator it) 
          { *p = std::move(*it); });
    }
  }

  static block_pointer 
  static_cast_block_pointer(block_base_pointer pbb) noexcept
  {
    return block_list::static_cast_block_pointer(pbb);
  }

  block_pointer create_new_available_block()
  {
    struct deleter
    {
      using pointer = block_pointer;
      void operator()(pointer p) noexcept { allocator_deallocate(al, p, 1); }
      block_allocator al;
    };

    std::unique_ptr<block, deleter> pb{allocator_allocate(al(), 1), {al()}}; 
    pb->mask = 0;
    allocator_rebind_t<Allocator, value_type> val(al());
    pb->data = allocator_allocate(val, N);
    blist.link_available_at_back(pb.get());
    ++num_blocks;
    return pb.release();
  }

  void delete_block(block_pointer pb) noexcept
  {
    allocator_rebind_t<Allocator, value_type> val(al());
    allocator_deallocate(val, pb->data, N);
    allocator_deallocate(al(), pb, 1);
  }

  BOOST_FORCEINLINE block_pointer retrieve_available_block(int& n)
  {
    if(blist.next_available != blist.header()){
      auto pb = static_cast_block_pointer(blist.next_available);
      n = detail::unchecked_countr_one(pb->mask);
      return pb;
    }
    else {
      n = 0;
      return create_new_available_block();
    }
  }

  size_type destroy_all_in_nonempty_block(block_pointer pb) noexcept
  {
    BOOST_ASSERT(pb->mask != 0);
    return destroy_all_in_nonempty_block(
      pb, std::is_trivially_destructible<T>{});
  }

  size_type destroy_all_in_nonempty_block(
    block_pointer pb, std::true_type /* trivially destructible */) noexcept
  {
    return (size_type)core::popcount(pb->mask);
  }

  size_type destroy_all_in_nonempty_block(
    block_pointer pb, std::false_type /* ~trivially destructible */) noexcept
  {
    size_type s = 0;
    auto      mask = pb->mask;
    do {
      auto n = detail::unchecked_countr_zero(mask);
      allocator_destroy(al(), pb->data + n);
      ++s;
      mask &= mask - 1;
    } while(mask);
    return s;
  }

  size_type destroy_all_in_full_block(block_pointer pb) noexcept
  {
    BOOST_ASSERT(pb->mask == full);
    for(int n = 0; n < N; ++n) {
      allocator_destroy(al(), boost::to_address(pb->data + n));
    }
    return (size_type)N;
  }

  void reset() noexcept
  {
    for(auto pbb = blist.next_available; pbb != blist.header(); ) {
      auto pb = static_cast_block_pointer(pbb);
      pbb = pb->next_available;
      BOOST_HUB_PREFETCH_BLOCK(pbb, block);
      if(pb->mask != 0) {
        destroy_all_in_nonempty_block(pb);
        blist.unlink(pb);
      }
      delete_block(pb);
    }
    /* full blocks remaining */
    for(auto pbb = blist.next; pbb != blist.header(); ) {
      BOOST_ASSERT(pbb->mask == full);
      auto pb = static_cast_block_pointer(pbb);
      pbb = pb->next;
      BOOST_HUB_PREFETCH_BLOCK(pbb, block);
      destroy_all_in_full_block(pb);
      delete_block(pb);
    }
    blist.reset();
    num_blocks = 0;
    size_ = 0;
  }

  template<typename Incrementable, typename Sentinel, typename Construct>
  void range_insert_impl(
    Incrementable first, Sentinel last, Construct construct)
  {
    while(first != last) {
      int  n;
      auto pb = retrieve_available_block(n);
      for(; ; ) {
        construct(boost::to_address(pb->data + n), first++);
        ++size_;
        if(BOOST_UNLIKELY(pb->mask == 0)) blist.link_at_back(pb);
        pb->mask |= pb->mask +1;
        if(pb->mask == full){
          blist.unlink_available(pb);
          break;
        }
        else if(first == last) return;
        n = detail::unchecked_countr_one(pb->mask);
      }
    }
  }

  template<
    typename Incrementable, typename Sentinel, 
    typename Construct, typename Insert
  >
  void range_assign_impl(
    Incrementable first, Sentinel last, Construct construct, Insert insert)
  {
    auto pbb = blist.next;
    int  n = 0;
    if(first != last) {
      /* Consume active blocks.
       * NB: when the available list is foward only, we need to purge it after
       * traversal cause unlink_available(pb) requires that pb be the first
       * available block, which is generally not the case when pb has been
       * reached through the _active_ list.
       */
#if !defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
      purge_unavailable_on_exit on_exit{*this}; (void)on_exit;
#endif

      for(; pbb != blist.header(); pbb = pbb->next, n = 0) {
        auto pb = static_cast_block_pointer(pbb);
        for(mask_type bit = 1; bit; bit <<= 1, ++n) {
          if(pb->mask & bit) { /* full slot */
            insert(boost::to_address(pb->data + n), first++);
          }
          else { /* empty slot */
            construct(boost::to_address(pb->data + n), first++);
            ++size_;
            pb->mask |= bit;
          }
#if defined(BOOST_HUB_ENABLE_BIDIRECTIONAL_AVAILABLE_LIST)
          if(pb->mask == full) blist.unlink_available(pb);
#endif
          if(first == last) goto exit;
        }
      }
    exit: ;
    }
    if(first != last) {
      /* all active blocks consumed, keep inserting */
      range_insert_impl(first, last, construct);
    }
    else{
      /* erase remaining original elements */
      auto it = (n == 0)? const_iterator{pbb}: ++const_iterator{pbb, n};
      erase(it, cend());
    }
  }

  void compact()
  {
    for(auto pbbx = blist.next_available; pbbx != blist.header(); ) {
      auto pbx = static_cast_block_pointer(pbbx);
      if(pbx->mask != 0) {
        auto pbby = pbbx->next_available;
        do{
          while(pbby->mask == 0) pbby = pbby->next_available;
          if(pbby == blist.header()) {
            compact(pbx);
            blist.unlink(pbx);
            blist.link_at_back(pbx);
            return;
          }
          else{
            auto pby = static_cast_block_pointer(pbby);
            compact(pbx,pby);
            if(pby->mask == 0) blist.unlink(pby);
          }
        }while(pbx->mask != full);
      }
      pbbx = pbx->next_available;
      blist.unlink_available(pbx);
    }
  }

  void compact(block_pointer pbx, block_pointer pby)
  {
    auto cx = core::popcount(pbx->mask),
         cy = core::popcount(pby->mask);
    if(cx < cy) {
      std::swap(cx, cy);
      swap_payload(*pbx, *pby);
    }
    auto c = (std::min)(N - cx, cy);
    while(c--) {
      auto n = detail::unchecked_countr_one(pbx->mask);
      auto m = N - 1 - detail::unchecked_countl_zero(pby->mask);
      allocator_construct(
        al(), boost::to_address(pbx->data + n), std::move(pby->data[m]));
      allocator_destroy(al(), boost::to_address(pby->data + m));
      pbx->mask |= pbx->mask + 1;
      pby->mask &= ~((mask_type)(1) << m);
    }
  }

  void compact(block_pointer pb)
  {
    for(; ;) {
      auto n = detail::unchecked_countr_one(pb->mask);
      auto m = N - 1 - detail::unchecked_countl_zero(pb->mask);
      if(n > m) return;
      allocator_construct(
        al(), boost::to_address(pb->data + n), std::move(pb->data[m]));
      allocator_destroy(al(), boost::to_address(pb->data + m));
      pb->mask |= pb->mask + 1;
      pb->mask &= ~((mask_type)(1) << m);
    }
  }

  block_list blist;
  size_type  num_blocks = 0;
  size_type  size_ = 0;
};

#if !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES)
template<
  typename InputIterator, 
  typename Allocator = std::allocator<
    typename std::iterator_traits<InputIterator>::value_type>
>
hub(InputIterator, InputIterator, Allocator = Allocator())
  -> hub<
    typename std::iterator_traits<InputIterator>::value_type, Allocator>;

#if !defined(BOOST_HUB_NO_RANGES)
template<
  std::ranges::input_range R,
  typename Allocator = std::allocator<std::ranges::range_value_t<R>>
>
hub(from_range_t, R&&, Allocator = Allocator())
  -> hub<std::ranges::range_value_t<R>, Allocator>;
#endif
#endif

template<typename T, typename Allocator>
void swap(hub<T, Allocator>& x, hub<T, Allocator>& y)
  noexcept(noexcept(x.swap(y)))
{
  x.swap(y);
}

template<typename T, typename Allocator, typename Predicate>
typename hub<T, Allocator>::size_type
erase_if(hub<T, Allocator>& x, Predicate pred)
{
  using size_type = typename hub<T, Allocator>::size_type;
  
  auto s = x.size();
  auto first = x.cbegin(), last = x.cend();
  while((first = std::find_if(first, last, pred)) != last) {
    first = x.erase(first, std::find_if_not(std::next(first), last, pred));
    if(first == last) break;
    ++first;
  }
  return (size_type)(s - x.size());
}

template<typename T, typename Allocator, typename U>
typename hub<T, Allocator>::size_type
erase(hub<T, Allocator>& x, const U& value)
{
  return erase_if(x, [&](const T& v) -> bool { return v == value; });
}

} /* namespace hubs */

} /* namespace boost */

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

#endif
