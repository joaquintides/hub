/* Tests for boost::container::hub slab-based block storage.
 *
 * Copyright 2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <algorithm>
#include <boost/container/hub.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

/* GCC on Darwin cannot parse the system <mach/message.h> header
 * (xnu_static_assert_struct_size uses Clang-only extensions).
 */
#if defined(__GNUC__) && !defined(__clang__) && defined(__APPLE__)
#define BOOST_CONTAINER_HUB_TEST_SLAB_NO_INTERPROCESS
#endif

#if !defined(BOOST_CONTAINER_HUB_TEST_SLAB_NO_INTERPROCESS)
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sstream>
#include <string>
#endif

/* enough elements to be well past the slab threshold (64 blocks) */
static constexpr std::size_t above_threshold = 20000;

/* counting allocator: verifies that all memory, slabs included, is obtained
 * from and returned to the user-provided allocator */

struct alloc_stats
{
  std::size_t allocations = 0;
  std::size_t deallocations = 0;
  std::size_t bytes_allocated = 0;
  std::size_t bytes_deallocated = 0;
  int         countdown_to_throw = 0;
};

template<typename T>
struct counting_allocator
{
  using value_type = T;

  counting_allocator(alloc_stats& stats_): stats{&stats_} {}
  template<typename U>
  counting_allocator(const counting_allocator<U>& x): stats{x.stats} {}

  T* allocate(std::size_t n)
  {
    if(stats->countdown_to_throw && !--stats->countdown_to_throw) {
      throw std::runtime_error("injected throw");
    }
    ++stats->allocations;
    stats->bytes_allocated += n * sizeof(T);
    return static_cast<T*>(
      ::operator new(n * sizeof(T), std::align_val_t{alignof(T)}));
  }
  void deallocate(T* p, std::size_t n) noexcept
  {
    ++stats->deallocations;
    stats->bytes_deallocated += n * sizeof(T);
    ::operator delete(p, std::align_val_t{alignof(T)});
  }

  template<typename U>
  bool operator==(const counting_allocator<U>& x) const
  { return stats == x.stats; }
  template<typename U>
  bool operator!=(const counting_allocator<U>& x) const
  { return stats != x.stats; }

  alloc_stats* stats;
};

static int live_tracked = 0;

struct tracked
{
  tracked(int n_): n{n_} { ++live_tracked; }
  tracked(const tracked& x): n{x.n} { ++live_tracked; }
  ~tracked() { --live_tracked; }
  operator int() const { return n; }
  int n;
};

using counted_hub = boost::container::hub<tracked, counting_allocator<tracked>>;

void test_allocator_routing()
{
  alloc_stats stats;
  {
    counted_hub h{counting_allocator<tracked>{stats}};
    for(std::size_t i = 0; i < above_threshold; ++i) h.insert((int)i);
    BOOST_TEST_EQ(h.size(), above_threshold);

    /* above the threshold, block storage comes in slab-sized allocations:
     * the total number of allocations must be far smaller than the number
     * of blocks (individual allocation would need 2 per block) */
    std::size_t blocks = h.capacity() / 64;
    BOOST_TEST(stats.allocations < 2 * 64 /* individual tier */ + blocks / 32);
  }
  BOOST_TEST_EQ(live_tracked, 0);
  BOOST_TEST_EQ(stats.allocations, stats.deallocations);
  BOOST_TEST_EQ(stats.bytes_allocated, stats.bytes_deallocated);
}

void test_above_threshold_ops()
{
  alloc_stats stats;
  {
    counted_hub h{counting_allocator<tracked>{stats}};
    std::multiset<int> ref;
    std::vector<std::pair<counted_hub::iterator, int>> its;
    std::mt19937_64 rng{731};
    for(int round = 0; round < 120000; ++round) {
      if(its.empty() || (rng() % 100) < 56) {
        int v = (int)(rng() % 100000);
        its.push_back({h.insert(v), v});
        ref.insert(v);
      }
      else {
        auto k = rng() % its.size();
        h.erase_void(its[k].first);
        ref.erase(ref.find(its[k].second));
        its[k] = its.back();
        its.pop_back();
      }
    }
    BOOST_TEST_EQ(h.size(), ref.size());
    std::multiset<int> got;
    for(const auto& x : h) got.insert((int)x);
    BOOST_TEST(got == ref);

    h.clear();
    BOOST_TEST_EQ(h.size(), 0u);
    for(int i = 0; i < 1000; ++i) h.insert(i);   /* reuse after clear */
    BOOST_TEST_EQ(h.size(), 1000u);
    h.trim_capacity();
    h.shrink_to_fit();
    long long sum = 0;
    for(const auto& x : h) sum += (int)x;
    BOOST_TEST_EQ(sum, 1000LL * 999 / 2);
  }
  BOOST_TEST_EQ(live_tracked, 0);
  BOOST_TEST_EQ(stats.bytes_allocated, stats.bytes_deallocated);
}

void test_splice_with_slabs()
{
  alloc_stats stats;
  {
    counting_allocator<tracked> al{stats};
    counted_hub b{al};
    {
      counted_hub a{al};
      for(std::size_t i = 0; i < above_threshold; ++i) a.insert((int)i);
      b.splice(a);
      BOOST_TEST_EQ(b.size(), above_threshold);
      BOOST_TEST_EQ(a.size(), 0u);
      /* donor keeps its carving slab and can reuse its spare capacity */
      for(int i = 0; i < 100; ++i) a.insert(-i);
      BOOST_TEST_EQ(a.size(), 100u);
    } /* donor destroyed first: slabs co-owned by b must survive */
    long long sum = 0;
    std::size_t cnt = 0;
    for(const auto& x : b) { sum += (int)x; ++cnt; }
    BOOST_TEST_EQ(cnt, above_threshold);
    BOOST_TEST_EQ(
      sum, (long long)above_threshold * (above_threshold - 1) / 2);
  }
  BOOST_TEST_EQ(live_tracked, 0);
  BOOST_TEST_EQ(stats.allocations, stats.deallocations);
  BOOST_TEST_EQ(stats.bytes_allocated, stats.bytes_deallocated);
}

void test_move_and_swap_with_slabs()
{
  alloc_stats stats;
  {
    counting_allocator<tracked> al{stats};
    counted_hub a{al};
    for(std::size_t i = 0; i < above_threshold; ++i) a.insert((int)i);
    counted_hub b{std::move(a)};
    BOOST_TEST_EQ(b.size(), above_threshold);
    counted_hub c{al};
    c = std::move(b);
    BOOST_TEST_EQ(c.size(), above_threshold);
    counted_hub d{al};
    for(int i = 0; i < 5000; ++i) d.insert(-i);
    c.swap(d);
    BOOST_TEST_EQ(c.size(), 5000u);
    BOOST_TEST_EQ(d.size(), above_threshold);
    /* all four containers keep working after the moves */
    c.insert(1); d.insert(1); a.insert(1); b.insert(1);
  }
  BOOST_TEST_EQ(live_tracked, 0);
  BOOST_TEST_EQ(stats.bytes_allocated, stats.bytes_deallocated);
}

void test_exception_safety_at_slab_allocation()
{
  alloc_stats stats;
  {
    counted_hub h{counting_allocator<tracked>{stats}};
    /* fill to just below a fresh slab allocation */
    for(std::size_t i = 0; i < 64 * 64; ++i) h.insert((int)i);
    while(h.size() < h.capacity()) h.insert(0);
    auto size0 = h.size();
    auto cap0 = h.capacity();
    auto allocs0 = stats.allocations;

    stats.countdown_to_throw = 1; /* next allocation (a slab) throws */
    BOOST_TEST_THROWS((void)h.insert(42), std::runtime_error);
    stats.countdown_to_throw = 0;

    BOOST_TEST_EQ(h.size(), size0);
    BOOST_TEST_EQ(h.capacity(), cap0);
    BOOST_TEST_EQ(stats.allocations, allocs0);

    h.insert(42); /* recovers normally */
    BOOST_TEST_EQ(h.size(), size0 + 1);
  }
  BOOST_TEST_EQ(live_tracked, 0);
  BOOST_TEST_EQ(stats.bytes_allocated, stats.bytes_deallocated);
}

#if !defined(BOOST_CONTAINER_HUB_TEST_SLAB_NO_INTERPROCESS)
void test_slabs_in_shared_memory()
{
  /* exercises slab carving through fancy pointers (offset_ptr) and a
   * stateful allocator */
  namespace bip = boost::interprocess;
  using segment_manager = bip::managed_shared_memory::segment_manager;
  using shared_int_allocator = bip::allocator<int, segment_manager>;
  using shared_int_hub = boost::container::hub<int, shared_int_allocator>;

  static auto segment_name_str =
    std::string("boost_hub_test_slab_shmem_segment") +
    to_string(boost::uuids::random_generator()());
  static auto segment_name = segment_name_str.c_str();
  static struct segment_remover
  {
    segment_remover() { bip::shared_memory_object::remove(segment_name); }
    ~segment_remover() { bip::shared_memory_object::remove(segment_name); }
  } remover;
  (void)remover;

  bip::managed_shared_memory segment(
    bip::create_only, segment_name, 8 * 1024 * 1024);
  shared_int_allocator al(segment.get_segment_manager());

  {
    shared_int_hub a{al}, b{al};
    for(std::size_t i = 0; i < 10000; ++i) a.insert((int)i);
    BOOST_TEST_EQ(a.size(), 10000u);
    long long sum = 0;
    for(const auto& x : a) sum += x;
    BOOST_TEST_EQ(sum, 10000LL * 9999 / 2);

    b.splice(a);
    BOOST_TEST_EQ(b.size(), 10000u);
    std::vector<shared_int_hub::iterator> its;
    for(auto it = b.begin(); it != b.end(); ++it) its.push_back(it);
    std::mt19937_64 rng{99};
    std::shuffle(its.begin(), its.end(), rng);
    for(std::size_t i = 0; i < its.size() / 2; ++i) b.erase_void(its[i]);
    BOOST_TEST_EQ(b.size(), 5000u);
  }
  /* both hubs destroyed: everything must have been returned to the
   * segment; a full-size fresh hub must fit again */
  {
    shared_int_hub c{al};
    for(std::size_t i = 0; i < 10000; ++i) c.insert((int)i);
    BOOST_TEST_EQ(c.size(), 10000u);
  }
}
#endif

int main()
{
  test_allocator_routing();
  test_above_threshold_ops();
  test_splice_with_slabs();
  test_move_and_swap_with_slabs();
  test_exception_safety_at_slab_allocation();
#if !defined(BOOST_CONTAINER_HUB_TEST_SLAB_NO_INTERPROCESS)
  test_slabs_in_shared_memory();
#endif
  return boost::report_errors();
}
