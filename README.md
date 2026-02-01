# Hub container

`boost::container::hub` (proposed for Boost.Container) is a nearly drop-in replacement
of [`std::hive`](https://eel.is/c++draft/sequences#hive) with better performance than
the current reference implementation of this standard container.

* [Motivation](#motivation)
* [Performance](#performance)
  * [GCC 15, x64](#gcc-15-x64)
  * [Clang 20, x64](#clang-20-x64)
  * [Clang 17, ARM64](#clang-17-arm64)
  * [VS 2022, x64](#vs-2002-x64)
  * [GCC 15, x86](#gcc-15-x86)
  * [Clang 20, x86](#clang-20-x86)
  * [VS 2022, x86](#vs-2022-x86)
* [Synopsis](#synopsis)

## Motivation

`std::hive` was [accepted into C++26](https://herbsutter.com/2025/02/17/trip-report-february-2025-iso-c-standards-meeting-hagenberg-austria)
in February 2025. As of this writing, no major standard library implementor is providing
this container yet, though the work to do so is ongoing. Matthew Bentley's
[`plf::hive`](https://github.com/mattreecebentley/plf_hive) is the de facto reference
implementation. Two important decisions in the [design of `plf::hive`](https://plflib.org/colony.htm#details)
are:

* As the size of the container grows, newly allocated element blocks get larger up to a
limit specified by the user and capped internally. This is done to increase cache locality
while keeping memory usage reasonable for small containers.
* Efficient iteration and location of available slots are served by a combination
of a [skipfield array](https://plflib.org/matt_bentley_-_the_low_complexity_jump-counting_pattern.pdf)
and a list of erased elements (the latter embedded into the memory of the erased elements
themselves).

This structure requires significant bookkeeping and introduces a minimum memory
overhead of at least one (and typically two) bytes per slot. The question arises of whether
we can come up with a more efficient alternative design.

The internal data structure of `boost::container::hub` is as follows:

![diagram](doc/img/data_structure.png)

* Active blocks are kept in an intrusive doubly-linked list. Block size
is fixed to 64 elements.
* Each block points to its associated element array and maintains a bitmask
of used slots.
* _Available_ blocks (those with at least one free slot) are kept in another
intrusive doubly-linked list (not shown in the diagram).

Blocks then hold five pointers (two intrusive lists plus a pointer to the
element array) and a mask of type `std::uint64_t`, yielding a total overhead of 
6 bits per slot (in 64-bit mode). Locating an occupied (resp. free) slot in a given
block can be effectively accomplished in constant time with
[`std::countr_zero(mask)`](https://en.cppreference.com/w/cpp/numeric/countr_zero.html)
(resp. `std::countr_one(mask)`). It is not hard to see that insertion, erasure and
iterator increment can also be implemented in (non-amortized) constant time.

## Deviations from `std::hive`

`boost::container::hub` does not conform to the specification of `std::hive` in
a few aspects:

* Minimum and maximum block size limits can't be specified and are fixed to 64.
`reshape` is not provided as it doesn't make sense when block capacity is fixed.
* `trim_capacity` is linear on the number of _available_ locks
(`std::hive::trim_capacity` is linear on the number of _reserved_ blocks,
i.e. those without any used slot).
* Iterators are not
[`three_way_comparable`](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable.html):
Making them so would require extra block metadata and bookkeeping, and this overhead
was not deemed worth imposing over the potential usefulness of having ordered
iterators.

The following functionality is specific to `boost::container::hub`:

* As cache locality is relatively poorer than that of `plf::hive`, which can use
much larger blocks, iteration performance may suffer. To partially alleviate this,
_internal visitation_ functions `visit`,  `visit_while`, `visit_all` and
`visit_all_while` are provided: these are more performant than regular external
iteration thanks to a combination of unrolling and prefetching techniques.
* `erase_void` is an alternative to `erase` that does not return an iterator
to the next element, thus saving some potential runtime overhead.

## Performance

Benchmarks of `boost::container::hub` vs. `plf::hive` are run as GitHub Actions jobs in a
[dedicated repo](https://github.com/boostorg/boost_hub_benchmarks). Execution times for
the following scenarios are measured:

* Insertion of _n_ elements in the container, random erasure of elements with
probability _r_ and insertion of elements until the size of the container becomes
_n_ again.
* The above, plus destruction of the container.
* Iterator-based traversal of the container after insertion of _n_ elements and
random erasure with probability _r_.
* Visitation-based traversal for `boost::container::hub` vs. iterator-based traversal
for `plf::hive`.
* Sorting the container after insertion of _n_ elements and
random erasure with probability _r_.

Benchmarks cover all the combinations of

* _n_ = 10<sup>3</sup>, 10<sup>4</sup>, ...,  10<sup>7</sup>,
* _r_ = 0, 0.1,  ..., 0.9,
* `sizeof(element)` = 16, 32, 64, 80.

Values show the relative execution time of `plf::hive` with respect to
`boost::container::hub` (e.g. "1.2" means `boost::container::hub` is 1.2
times faster than `plf::hive`).

## Synopsis

```cpp
// #include <boost/container/hub.hpp>

namespace boost {
namespace container {

using from_range_t = /* implementation-defined */;
inline constexpr from_range_t from_range {};

template<typename T, typename Allocator = std::allocator<T>>
class hub
{
public:
  // types
  using value_type = T;
  using allocator_type = Allocator;
  using pointer = std::allocator_traits<Allocator>::pointer;
  using const_pointer = std::allocator_traits<Allocator>::const_pointer;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = /* implementation-defined */;
  using difference_type = /* implementation-defined */;
  using iterator = /* implementation-defined */;
  using const_iterator = /* implementation-defined */;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // construct/copy/destroy
  hub() noexcept(noexcept(Allocator()));
  explicit hub(const Allocator&) noexcept;
  explicit hub(size_type n, const Allocator& = Allocator());
  hub(size_type n, const T& value, const Allocator& = Allocator());
  template<typename InputIterator>
    hub(InputIterator first, InputIterator last, const Allocator& = Allocator());
  template</* container-compatible-range<T> */ R>
    hub(from_range_t, R&& rg, const Allocator& = Allocator());
  hub(const hub& x);
  hub(hub&&) noexcept;
  hub(const hub& x, const std::type_identity_t<Allocator>& alloc);
  hub(hub&&, const std::type_identity_t<Allocator>& alloc);
  hub(std::initializer_list<T> il, const Allocator& = Allocator());
  ~hub();

  hub& operator=(const hub& x);
  hub& operator=(hub&& x)
    noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
             std::allocator_traits<Allocator>::is_always_equal::value);
  hub& operator=(std::initializer_list<T>);
  template<typename InputIterator>
    void assign(InputIterator first, InputIterator last);
  template</* container-compatible-range<T> */ R>
    void assign_range(R&& rg);
  void assign(size_type n, const T& t);
  void assign(std::initializer_list<T>);
  allocator_type get_allocator() const noexcept;

  // iterators
  iterator                begin() noexcept;
  const_iterator          begin() const noexcept;
  iterator                end() noexcept;
  const_iterator          end() const noexcept;
  reverse_iterator        rbegin() noexcept;
  const_reverse_iterator  rbegin() const noexcept;
  reverse_iterator        rend() noexcept;
  const_reverse_iterator  rend() const noexcept;
  const_iterator          cbegin() const noexcept;
  const_iterator          cend() const noexcept;
  const_reverse_iterator  crbegin() const noexcept;
  const_reverse_iterator  crend() const noexcept;

  // capacity
  bool empty() const noexcept;
  size_type size() const noexcept;
  size_type max_size() const noexcept;
  size_type capacity() const noexcept;
  void reserve(size_type n);
  void shrink_to_fit();
  void trim_capacity() noexcept;
  void trim_capacity(size_type n) noexcept;

  // modifiers
  template<typename... Args>
    iterator emplace(Args&&... args);
  template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args);
  iterator insert(const T& x);
  iterator insert(T&& x);
  iterator insert(const_iterator hint, const T& x);
  iterator insert(const_iterator hint, T&& x);
  void insert(std::initializer_list<T> il);
  template</* container-compatible-range<T> */ R>
    void insert_range(R&& rg);
  template<typename InputIterator>
    void insert(InputIterator first, InputIterator last);
  void insert(size_type n, const T& x);

  iterator erase(const_iterator position);
  void erase_void(const_iterator position);
  iterator erase(const_iterator first, const_iterator last);
  void swap(hub&)
    noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value ||
             std::allocator_traits<Allocator>::is_always_equal::value);
  void clear() noexcept;

  // std::hive operations
  void splice(hub& x);
  void splice(hub&& x);
  template<typename BinaryPredicate = std::equal_to<T>>
    size_type unique(BinaryPredicate pred = BinaryPredicate());

  template<typename Compare = std::less<T>>
    void sort(Compare comp = Compare());

  iterator get_iterator(const_pointer p) noexcept;
  const_iterator get_iterator(const_pointer p) const noexcept;

  // internal visitation
  template<typename F>
    void visit(iterator first, iterator last, F f);
  template<typename F>
    void visit(const_iterator first, const_iterator last, F f) const;
  template<typename F>
    iterator visit_while(iterator first, iterator last, F f);
  template<typename F>
    const_iterator visit_while(const_iterator first, const_iterator last, F f) const;
  template<typename F>
    void visit_all(F f);
  template<typename F>
    void visit_all(F f) const;
  template<typename F>
    iterator visit_all_while(F f);
  template<typename F>
    const_iterator visit_all_while(F f) const;
};

template<
  typename InputIterator,
  typename Allocator = std::allocator<typename std::iterator_traits<InputIterator>::value_type>
>
  hub(InputIterator, InputIterator, Allocator = Allocator())
    -> hub<iter-value-type<InputIterator>, Allocator>;

template<std::ranges::input_range R, typename Allocator = allocator<std::ranges::range_value_t<R>>>
  hub(from_range_t, R&&, Allocator = Allocator())
    -> hub<std::ranges::range_value_t<R>, Allocator>;

template<typename T, typename Allocator>
  void swap(hub<T, Allocator>& x, hub<T, Allocator>& y) noexcept(noexcept(x.swap(y)));

template<typename T, typename Allocator, typename U = T>
  typename hub<T, Allocator>::size_type
    erase(hub<T, Allocator>& x, const U& value);

template<typename T, typename Allocator, typename Predicate>
  typename hub<T, Allocator>::size_type
    erase_if(hub<T, Allocator>& x, Predicate pred);

namespace pmr {

template<typename T>
  using hub = boost::container::hub<T, std::pmr::polymorphic_allocator<T>>;

} // namespace pmr


} // namespace container
} // namespace boost
```

* The library requires C++11 at a minimum.
  * User defined deduction guides are ony available if the compiler supports CTAD. 
  * range-related operations are only available if the standard library provides
    `<ranges>` and `<concepts>`.
  * `boost::container::pmr::hub` is only available if the standard library provides
    `<memory_resource>`.
* `from_range_t` is equal to  C++23 [`std::from_range_t`](https://en.cppreference.com/w/cpp/ranges/from_range.html)
if this is provided; otherwise, it is a different type with the same characteristics.
