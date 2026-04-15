# Hub container

[![Branch](https://img.shields.io/badge/branch-develop-brightgreen.svg)](https://github.com/joaquintides/hub/tree/develop) [![CI](https://github.com/joaquintides/hub/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/joaquintides/hub/actions/workflows/ci.yml) [![Coverage](https://joaquintides.github.io/hub/develop/gcovr/badges/coverage-lines.svg)](https://joaquintides.github.io/hub/develop/gcovr/index.html) <br/>
[![BSL 1.0](https://img.shields.io/badge/license-BSL_1.0-blue.svg)](https://www.boost.org/users/license.html) <img alt="C++11 required" src="https://img.shields.io/badge/standard-C%2b%2b11-blue.svg"> <img alt="Header-only library" src="https://img.shields.io/badge/build-header--only-blue.svg">

`boost::container::hub` (proposed for Boost.Container) is a nearly drop-in replacement
of [`std::hive`](https://eel.is/c++draft/sequences#hive) with a more compact design than
the current reference implementation of this standard container.

* [Introduction](#introduction)
* [Getting started](#getting-started)
* [Tutorial](#tutorial)
  * [Unordered insertion](#unordered-insertion)
  * [Capacity](#capacity)
  * [`std::hive` operations](#stdhive-operations)
  * [Internal visitation](#internal-visitation)
  * [Debugging](#debugging)
    * [Visual Studio Natvis](#visual-studio-natvis)
    * [GDB Pretty-Printer](#gdb-pretty-printer)
* [Comparison with `std::hive`](#comparison-with-stdhive)
  * [Motivation for a novel data structure](#motivation-for-a-novel-data-structure)
  * [Deviations from `std::hive`](#deviations-from-stdhive)
* [Performance](#performance)
  * [GCC 15, x64](#gcc-15-x64)
  * [Clang 20, x64](#clang-20-x64)
  * [Clang 17, ARM64](#clang-17-arm64)
  * [VS 2022, x64](#vs-2022-x64)
  * [GCC 15, x86](#gcc-15-x86)
  * [Clang 20, x86](#clang-20-x86)
  * [VS 2022, x86](#vs-2022-x86)
* [Reference](#reference)
  * [`<boost/container/hub.hpp>`](#boostcontainerhubhpp)
  * [Class template `boost::container::hub`](#class-template-boostcontainerhub)
    * [Synopsis](#synopsis)
    * [Description](#description)
    * [Constructors, copy and assignment](#constructors-copy-and-assignment)
    * [Capacity](#ref-capacity)
    * [Modifiers](#modifiers)
    * [`std::hive` operations](#ref-stdhive-operations)
    * [Internal visitation](#ref-internal-visitation)
    * [Erasure](#erasure)

## Introduction

`boost::container::hub` is a sequence container with O(1) insertion and erasure and _element stability_: 
pointers/iterators to an element remain valid as long as the element is not erased.

```cpp
#include <boost/container/hub.hpp>
#include <cassert>

int main()
{
  boost::container::hub<int> h;

  // Insert some elements and keep an iterator to one of them
  for(int i = 0; i < 100; ++i) h.insert(i);
  auto it = h.insert(100);
  for(int i = 101; i < 200; ++i) h.insert(i);

  // Erase some of the elements
  erase_if(h, [](int x) { return x % 2 != 0;});
  assert(*it = 100); // iterator still valid

  // Insert many more elements
  for(int i = 200; i < 10000; ++i) h.insert(i);
  assert(*it = 100); // iterator still valid
}
```

The observant reader may retort that `std::list` is also stable and provides O(1) insertion/erasure:
the key difference is that `boost::container::hub` is orders of magnitude faster because memory is allocated
in chunks of 64 contiguous elements, which amortizes allocation costs and provides some degree of
cache locality. An important tradeoff when using `boost::container::hub` is the fact that the user can't
control the position where a new element will be inserted: `boost::container::hub` reuses the memory
addresses of previously erased elements to maximize performance and keep the data structure as compact
as possible.

`boost::container::hub` is very similar but not entirely equivalent to C++26
[`std::hive`](https://eel.is/c++draft/sequences#hive) (hence the different naming).
Consult the section ["Comparison with `std::hive`"](#comparison-with-stdhive) for details.

The primary use case for `boost::container::hub`, `std::hive` and similar containers such
as _slot maps_ is in high-performance scenarios where elements are created and destroyed frequently,
insertion order is not relevant and pointer/iterator stability is required: game entity systems,
particle simulation and HFT come to mind.

## Getting started

`boost::container::hub` depends on Boost. Consult the website [section](https://www.boost.org/doc/user-guide/getting-started.html) on how
to install the entire Boost project or only the exact dependencies of `boost::container::hub`
(`assert`, `config`, `core` and `throw_exception`).

This is a header-only library, so no additional build phase is needed. C++11 or later required.
The library has been verified to work with GCC 4.8, Clang 3.5 and Visual Studio 2022/MSVC 14.3 
(and later versions of those). You can check that your environment is correctly set up by
compiling the example program shown above.

## Tutorial

If you're familiar with STL sequence containers (`std::list`,  `std::vector`),
getting used to `boost::container::hub` is entirely straightforward as its API is
mostly ananalogus. The key characteristics that set this container apart are:

* Pointers and iterators to an element remain valid as long as the element is not
erased. `hub` will _not_ reallocate elements as it grows in size.
* Insertion and erasure are constant-time and very fast. Memory is allocated in
blocks with capacity for 64 elements each, and the container keeps track of
available positions, including those of erased elements, to use them for further
insertions and keep the number of memory allocations to the minimum possible.

### Unordered insertion

As a result of its memory reuse policy, users generally can't control the resulting
insertion order in a `hub`:

```cpp
boost::container::hub<int> h = {0, 1, 2};
h.erase(h.begin());
h.insert({3, 4, 5});
for(const auto& x: h) std::cout << x << " ";
```
Output
```
3 1 2 4 5
```

In the example, `h.erase(h.begin())` generates an available position where
`0` used to be, and this is where `3` goes in when inserting `{3, 4, 5}`,
rather than after `2`.

### Capacity

`reserve` can be used to preallocate memory blocks before insertion:

```cpp
boost::container::hub<int> h;
h.reserve(1000); // capacity() == 1024 (rounded up to 64)
for(int i = 0; i < 500; ++i) h.insert(i); // won't allocate (500 <= 1024)
```

In the example, `h` ends up with ⌈500/64⌉ = 8 non-empty blocks and
1024/64 - 8 = 8 empty blocks (also called _reserved_ blocks). Empty blocks
can be deallocated as follows:

```cpp
h.trim_capacity(750); // capacity() == 768
h.trim_capacity();    // ~trim_capacity(0), capacity() == 512
```

Obviously, `h.trim_capacity()` doesn't bring the capacity down to zero
because the `hub` contains 500 elements.

After erasures, a `hub` may contain "holes" or available positions in
non-empty blocks that can't be trimmed further. `shrink_to_fit` reallocates
elements so that they occupy the minimum possible number of blocks, and
then deallocates the remaining blocks:

```cpp
erase_if(h, [](int x) { return x % 2 != 0; }); // erase odd values, size() == 250
h.shrink_to_fit(); // capacity() == ceil(250/64) * 64 = 256
for(const auto& x: h) std::cout << x << " ";
```
Output:
```
0 126 2 124 4 122 6 120 8 118 10...
```
Note how `shrink_to_fit` has reallocated the elements `126`, `124`, etc. so that
they go in the available positions previously occupied by odd values.

### `std::hive` operations

`boost::container::hub` provides operations specific to C++26 `std::hive`:

```cpp
boost::container::hub<int> h1 = {0, 2, 3, 4, 6},
                           h2 = {1, 4, 6, 7, 9};
h1.splice(h2); // transfer non-empty blocks from h2 to h1 (no rellocation)
h1.sort();     // sorts the values (reallocates)
h1.unique();   // erase repeated, consecutive values
```

A slightly more interesting operation is `get_iterator`:

```cpp
boost::container::hub<int> h;
//...
int* p = std::addressof(*h.insert(50));
//...
boost::container::hub<int>::iterator it = h.get_iterator(p);
h.erase(it); // erase the element (couldn't be done drectly with p)
```

`get_iterator` returns an iterator after a pointer to a valid element of the
`hub`. This can be useful in legacy scenarios where elements of the container
are externally tracked via pointers, or for encapsulation purposes, or
to save memory (`hub` iterators typically are 16 bytes in size). Note, however,
that `get_iterator` is not cheap: execution is linear on the number of
non-empty blocks.

### Internal visitation

The following, typical processing loop:

```cpp
boost::container::hub<int> h;
//...
for(auto& x: h) x *= 2;
```

Can also be written as:
```cpp
h.visit_all([](auto& x) { x *= 2; });
```

Although functionally equivalent to the classical loop, `visit_all` is generally
faster as it is implemented with a combination of loop unrolling and prefetching
techniques. Speedups can be as high as 1.75x. Consult the [performance](#performance)
section for a comparison of execution speeds. Consult the
[reference](#ref-internal-visitation) for documentation on variations of
`visit_all` (`visit`, `visit_while`, `visit_all_while`).

### Debugging
#### Visual Studio Natvis

Add the [`boost_hub.natvis`](extra/boost_hub.natvis) visualizer to your project to allow
for user-friendly inspection of `boost::container::hub`s.

![Natvis window](doc/img/natvis.png)

#### GDB Pretty-Printer

`boost::container::hub` comes with a dedicated 
[pretty-printer](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Pretty-Printing.html#Pretty-Printing)
for visual inspection when debugging with GDB:

```
(gdb) print h
$1 = boost::container::hub with {size = 7, capacity = 1024} = {0, 23, 1, 100, 10, 2, 42}
(gdb) print h[3]
$2 = 100
```

Remember to enable pretty-printing in GDB (typically a one-time setup):

```
(gdb) set print pretty on
```

And load the [`boost_hub_printers.py`](extra/boost_hub_printers.py) script before variable inspection:

```
(gdb) source <path-to-hub-repo>/extra/boost_hub_printers.py
```

## Comparison with `std::hive`
### Motivation for a novel data structure

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

### Deviations from `std::hive`

`boost::container::hub` does not conform to the specification of `std::hive` in
a few aspects:

* Minimum and maximum block size limits can't be specified and are fixed to 64.
`reshape` is not provided as it doesn't make sense when block capacity is fixed.
* `trim_capacity` is linear on the number of _available_ blocks
(`std::hive::trim_capacity` is linear on the number of _reserved_ blocks,
i.e. those without any used slot).
* Iterators are not
[`three_way_comparable`](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable.html):
Making them so would require extra block metadata and bookkeeping, and this overhead
was not deemed worth imposing over the potential usefulness of having ordered
iterators.
* No operations are marked `constexpr`.

The following functionality is specific to `boost::container::hub`:

* As cache locality is relatively poorer than that of `plf::hive`, which can use
much larger blocks, iteration performance may suffer. To partially alleviate this,
_internal visitation_ functions `visit`,  `visit_while`, `visit_all` and
`visit_all_while` are provided: these are more performant than regular external
iteration thanks to a combination of unrolling and prefetching techniques.
* `erase_void` is an alternative to `erase` that does not return an iterator
to the next element, thus saving some potential runtime overhead.
* The `end` iterator is stable and non-transferable, whereas for `std::hive`
the `end` iterator is invalidated upon any insertion or the erasure of the last
element (briefly put,
`boost::container::hub::end` behaves like `std::list::end` whereas
`std::hive::end` behaves like `std::vector::end`). Technically, this is
not a non-conformance but rather an extension to the specification of
`std::hive`.

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

### GCC 15, x64
<!--gcc-x64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.24 2.41 1.24 1.37 1.14 | 1.26 1.19 1.16 1.22 1.45 | 1.00 1.03 1.03 1.03 1.03 | 1.77 1.97 1.97 1.93 1.59 | 1.06 1.02 1.02 1.01 1.00 |
| 0.1        | 1.27 1.20 1.26 1.69 1.53 | 1.23 1.17 1.20 1.45 1.55 | 1.03 1.04 1.03 1.03 1.11 | 1.70 1.75 1.76 1.76 1.52 | 1.05 1.00 1.01 1.00 0.98 |
| 0.2        | 1.23 1.23 1.33 1.70 1.65 | 1.21 1.20 1.28 1.80 1.77 | 1.05 1.02 1.03 1.02 1.02 | 1.66 1.73 1.74 1.76 1.35 | 1.05 1.00 1.01 0.99 0.97 |
| 0.3        | 1.27 1.31 1.43 1.95 1.96 | 1.24 1.27 1.40 1.85 1.86 | 1.03 1.02 1.01 1.02 0.99 | 1.75 1.75 1.70 1.70 1.35 | 1.06 1.01 1.01 0.99 0.96 |
| 0.4        | 1.39 1.47 1.52 1.84 2.07 | 1.32 1.32 1.50 1.88 2.02 | 1.02 1.00 1.00 0.99 0.96 | 1.88 1.76 1.64 1.64 1.22 | 1.06 1.01 1.00 0.98 0.94 |
| 0.5        | 1.42 1.47 1.62 1.67 2.30 | 1.41 1.54 1.60 1.79 2.23 | 1.13 1.00 0.98 0.99 0.91 | 1.81 1.84 1.47 1.58 1.14 | 1.06 1.01 1.00 0.98 0.93 |
| 0.6        | 1.50 1.92 1.74 1.90 2.46 | 1.55 1.68 1.71 1.91 2.29 | 1.15 1.02 0.96 0.96 0.90 | 1.82 1.92 1.61 1.56 1.02 | 1.04 1.07 0.99 0.96 0.92 |
| 0.7        | 1.54 1.96 1.86 2.47 2.51 | 1.49 1.75 2.04 2.25 2.48 | 1.36 1.12 0.94 0.94 0.90 | 1.85 2.00 1.65 1.51 1.00 | 1.04 1.36 0.98 0.95 0.90 |
| 0.8        | 1.57 2.14 1.95 2.31 2.58 | 1.53 1.86 1.91 2.18 2.47 | 1.42 1.52 1.03 0.90 0.91 | 1.84 2.07 1.19 1.17 0.91 | 0.99 1.05 0.96 0.92 0.84 |
| 0.9        | 1.58 2.08 2.07 2.31 2.57 | 1.75 1.78 1.95 2.01 2.32 | 1.34 1.55 1.16 0.88 0.89 | 1.62 1.89 1.32 1.25 0.97 | 0.89 0.93 0.92 0.84 0.75 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_16.txt-->
<!--gcc-x64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.01 2.37 2.49 2.20 1.50 | 0.99 0.98 0.99 2.03 1.56 | 1.07 1.04 1.04 0.98 0.97 | 1.87 1.94 1.96 1.51 1.17 | 1.00 1.01 1.00 1.59 2.03 |
| 0.1        | 1.04 1.02 1.10 2.33 1.58 | 0.99 1.00 1.05 2.19 1.64 | 1.08 1.04 1.05 0.97 0.97 | 1.75 1.74 1.73 1.37 1.11 | 1.01 0.99 1.00 1.34 1.97 |
| 0.2        | 1.03 1.04 1.17 2.42 1.82 | 0.98 1.01 1.12 2.27 1.80 | 1.10 1.02 1.04 0.99 0.95 | 1.73 1.78 1.70 1.33 1.13 | 0.98 0.99 0.99 1.38 1.83 |
| 0.3        | 1.05 1.09 1.25 2.52 1.88 | 0.99 1.03 1.20 2.28 1.92 | 1.08 1.05 1.03 0.98 1.03 | 1.80 1.72 1.69 1.26 1.11 | 0.99 0.99 0.99 1.16 1.87 |
| 0.4        | 1.12 1.18 1.33 2.61 2.08 | 1.08 1.13 1.29 2.31 2.14 | 1.10 1.03 1.02 0.99 0.93 | 1.88 1.82 1.64 1.17 1.09 | 1.00 0.98 0.99 1.37 1.77 |
| 0.5        | 1.19 1.25 1.42 2.67 2.12 | 1.17 1.25 1.37 2.55 2.34 | 1.24 1.02 1.02 0.96 0.88 | 1.94 1.90 1.48 1.10 1.02 | 0.97 0.97 0.99 1.32 1.63 |
| 0.6        | 1.26 1.41 1.53 2.77 2.28 | 1.25 1.38 1.46 2.66 2.43 | 1.26 1.05 1.01 0.93 0.92 | 2.01 1.95 1.22 1.07 0.91 | 0.98 0.97 0.99 0.98 1.57 |
| 0.7        | 1.38 1.64 1.64 2.73 2.26 | 1.33 1.60 1.59 2.69 2.47 | 1.48 1.18 1.00 0.86 0.93 | 2.02 1.83 1.08 0.98 0.88 | 0.93 1.53 0.99 0.94 1.53 |
| 0.8        | 1.45 1.74 1.78 2.93 2.22 | 1.41 1.65 1.72 2.75 2.54 | 1.44 1.51 1.02 0.99 0.93 | 1.87 1.87 1.10 1.17 1.00 | 0.89 1.02 0.97 0.85 1.40 |
| 0.9        | 1.58 1.74 1.91 3.02 2.41 | 1.46 1.65 1.81 2.56 2.61 | 1.38 1.66 1.10 0.78 0.91 | 1.66 1.96 1.34 1.14 0.95 | 0.76 0.77 0.91 0.86 1.03 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_32.txt-->
<!--gcc-x64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.02 2.91 2.91 2.69 1.96 | 1.00 0.95 0.98 2.52 1.91 | 1.08 1.05 1.03 0.94 0.91 | 1.85 1.74 1.42 1.01 0.98 | 1.00 1.00 1.01 1.60 1.90 |
| 0.1        | 1.02 1.02 1.08 2.49 1.94 | 1.00 0.96 1.04 2.64 1.73 | 1.15 1.09 1.07 0.92 0.92 | 1.85 1.56 1.34 1.01 0.96 | 1.01 1.00 1.03 1.34 1.75 |
| 0.2        | 1.02 1.04 1.15 2.58 2.07 | 1.00 0.99 1.11 2.87 2.01 | 1.17 1.08 1.06 0.88 0.92 | 1.85 1.45 1.37 0.99 0.94 | 1.01 0.99 1.03 1.38 1.67 |
| 0.3        | 1.03 1.10 1.22 2.62 2.19 | 1.02 1.05 1.18 3.00 2.02 | 1.17 1.11 1.08 0.99 0.93 | 1.84 1.34 1.13 0.99 0.91 | 1.00 1.00 1.00 1.16 1.61 |
| 0.4        | 1.09 1.18 1.29 2.54 2.19 | 1.09 1.10 1.27 2.45 2.42 | 1.14 1.03 0.99 0.98 0.94 | 1.82 1.23 1.10 0.93 0.90 | 0.99 0.99 1.00 1.13 1.57 |
| 0.5        | 1.16 1.22 1.35 2.66 2.39 | 1.10 1.27 1.32 2.64 2.39 | 1.32 1.01 1.00 1.00 0.94 | 1.81 1.36 1.08 0.99 0.92 | 0.97 1.00 0.99 1.06 1.48 |
| 0.6        | 1.22 1.46 1.42 2.60 2.47 | 1.21 1.41 1.41 2.86 2.56 | 1.28 1.04 0.94 0.91 0.92 | 1.88 1.67 1.22 1.05 1.02 | 0.97 1.03 0.98 0.92 1.37 |
| 0.7        | 1.28 1.51 1.50 2.82 2.52 | 1.18 1.42 1.47 2.68 2.62 | 1.51 1.17 0.92 0.89 0.93 | 1.95 1.84 1.17 1.11 1.02 | 0.95 1.55 0.97 0.82 1.26 |
| 0.8        | 1.34 1.52 1.58 2.75 2.47 | 1.30 1.47 1.54 2.94 2.68 | 1.50 1.50 0.95 1.06 0.90 | 1.90 2.07 1.25 1.31 0.97 | 0.92 1.01 0.96 0.73 1.16 |
| 0.9        | 1.42 1.51 1.67 2.98 2.70 | 1.35 1.44 1.56 3.02 2.66 | 1.35 1.71 1.09 0.76 0.89 | 1.53 2.09 1.40 1.09 0.96 | 0.76 0.79 0.91 0.86 0.94 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_64.txt-->
<!--gcc-x64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.05 3.48 3.35 3.17 2.18 | 1.01 0.97 0.98 3.28 2.32 | 1.04 1.01 0.99 0.90 0.90 | 1.45 1.24 1.22 0.90 0.98 | 0.99 1.00 0.99 1.40 1.81 |
| 0.1        | 1.03 1.02 1.08 2.80 2.14 | 0.99 0.97 1.03 2.95 2.20 | 1.09 1.04 1.01 0.89 0.90 | 1.43 1.16 1.11 0.85 0.93 | 0.98 0.99 0.98 1.47 1.70 |
| 0.2        | 1.01 1.05 1.15 2.87 2.25 | 0.99 1.00 1.11 3.01 2.38 | 1.11 1.04 1.00 0.94 0.91 | 1.44 1.07 1.03 0.94 0.90 | 0.99 1.00 0.98 1.20 1.64 |
| 0.3        | 1.01 1.11 1.23 2.88 2.38 | 0.99 1.05 1.17 2.92 2.39 | 1.10 1.02 0.99 0.93 0.93 | 1.50 1.02 0.97 0.88 0.91 | 0.99 0.99 0.99 1.14 1.55 |
| 0.4        | 1.11 1.18 1.31 2.89 2.46 | 1.03 1.10 1.25 2.88 2.62 | 1.08 0.96 0.96 0.86 1.03 | 1.50 1.02 0.98 0.90 0.93 | 0.99 0.99 0.98 1.14 1.52 |
| 0.5        | 1.16 1.38 1.39 2.89 2.53 | 1.08 1.31 1.33 2.95 2.58 | 1.24 0.82 0.96 0.99 0.96 | 1.52 1.13 0.99 0.95 0.93 | 0.98 0.99 0.98 1.15 1.40 |
| 0.6        | 1.18 1.42 1.48 2.83 2.58 | 1.15 1.35 1.40 2.99 2.56 | 1.23 1.04 0.88 0.94 0.96 | 1.54 1.48 1.07 0.94 0.96 | 0.96 0.98 0.98 0.91 1.33 |
| 0.7        | 1.23 1.53 1.57 2.92 2.68 | 1.17 1.47 1.49 2.99 2.73 | 1.33 1.11 0.97 0.95 0.94 | 1.48 1.59 1.13 1.16 0.95 | 0.95 1.49 0.99 0.99 1.29 |
| 0.8        | 1.33 1.59 1.68 3.01 2.83 | 1.19 1.47 1.55 3.12 2.78 | 1.39 1.42 0.91 0.82 0.83 | 1.59 1.76 1.13 0.90 0.94 | 0.89 1.01 0.95 0.81 1.17 |
| 0.9        | 1.38 1.56 1.75 2.89 2.70 | 1.23 1.45 1.59 3.04 2.67 | 1.29 1.59 1.06 0.71 0.85 | 1.44 1.94 1.13 0.88 0.91 | 0.75 0.79 0.90 0.79 0.93 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_80.txt-->

### Clang 20, x64
<!--clang-x64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.46 2.76 1.36 1.73 1.33 | 1.62 1.43 1.46 1.53 1.33 | 1.32 1.35 1.34 1.31 1.26 | 1.97 2.13 2.12 2.07 1.83 | 1.05 1.03 1.01 1.00 0.99 |
| 0.1        | 1.49 1.42 1.39 1.79 1.36 | 1.63 1.50 1.48 1.82 1.41 | 1.33 1.35 1.34 1.30 1.21 | 1.85 1.80 1.76 1.73 1.57 | 1.02 0.99 0.99 0.97 0.95 |
| 0.2        | 1.46 1.49 1.56 2.39 1.60 | 1.66 1.52 1.62 2.29 1.65 | 1.33 1.32 1.32 1.31 1.19 | 1.82 1.76 1.73 1.66 1.51 | 1.03 1.00 0.99 0.97 0.95 |
| 0.3        | 1.54 1.64 1.76 2.66 1.74 | 1.78 1.65 1.76 2.68 1.77 | 1.30 1.31 1.29 1.25 1.28 | 1.90 1.74 1.69 1.59 1.38 | 1.02 0.99 0.99 0.96 0.94 |
| 0.4        | 1.57 1.74 1.91 2.82 1.91 | 1.88 1.75 1.90 2.80 2.00 | 1.26 1.28 1.27 1.25 1.06 | 2.12 1.83 1.64 1.46 1.23 | 1.02 1.02 0.99 0.96 0.93 |
| 0.5        | 1.75 1.86 2.03 3.08 2.11 | 1.96 1.90 2.04 3.10 2.15 | 1.26 1.25 1.24 1.21 1.13 | 2.06 1.96 1.61 1.35 1.22 | 1.02 1.03 0.98 0.96 0.92 |
| 0.6        | 1.71 2.07 2.22 3.00 2.17 | 2.07 2.13 2.21 3.21 2.22 | 1.37 1.23 1.19 1.10 0.99 | 2.09 2.19 1.62 1.41 1.15 | 1.01 1.05 0.97 0.94 0.90 |
| 0.7        | 1.74 2.12 2.40 3.44 2.47 | 2.17 2.14 2.40 3.31 2.43 | 1.63 1.30 1.13 1.07 1.06 | 2.09 2.36 1.68 1.34 1.09 | 1.00 1.56 0.95 0.92 0.87 |
| 0.8        | 1.81 2.11 2.55 3.16 2.25 | 2.28 2.14 2.55 3.40 2.47 | 1.68 1.75 1.15 1.13 0.97 | 2.19 2.28 1.46 1.20 0.94 | 0.95 1.05 0.94 0.89 0.83 |
| 0.9        | 1.83 2.13 2.66 3.39 2.33 | 2.33 2.20 2.67 3.18 2.31 | 1.42 1.72 1.21 0.84 0.98 | 1.78 2.04 1.42 1.04 1.25 | 0.85 0.82 0.91 0.82 0.73 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_16.txt-->
<!--clang-x64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.48 3.46 3.44 2.46 1.81 | 1.56 1.44 1.43 2.63 1.82 | 1.32 1.33 1.32 1.09 1.07 | 2.01 2.12 2.11 1.31 1.22 | 1.12 1.02 1.02 2.18 2.51 |
| 0.1        | 1.54 1.45 1.46 2.49 1.65 | 1.59 1.45 1.43 2.42 1.74 | 1.33 1.34 1.32 1.14 1.02 | 1.85 1.79 1.76 1.27 1.12 | 1.09 1.01 1.01 2.18 2.40 |
| 0.2        | 1.57 1.50 1.61 2.91 1.94 | 1.62 1.50 1.59 2.92 1.89 | 1.34 1.30 1.30 1.08 1.03 | 1.83 1.74 1.72 1.16 1.21 | 1.11 1.01 0.99 2.04 2.31 |
| 0.3        | 1.60 1.64 1.79 3.40 2.06 | 1.71 1.64 1.79 3.29 2.03 | 1.31 1.29 1.28 1.05 1.03 | 1.94 1.73 1.67 1.17 1.10 | 1.09 1.00 1.00 1.97 2.23 |
| 0.4        | 1.78 1.76 1.90 3.36 2.18 | 1.83 1.75 1.89 3.47 2.19 | 1.28 1.26 1.25 0.96 0.98 | 2.10 1.81 1.63 1.16 1.07 | 1.10 1.02 1.00 1.88 2.09 |
| 0.5        | 1.92 1.92 2.06 3.98 2.33 | 1.95 1.85 2.02 3.86 2.26 | 1.33 1.23 1.21 1.04 1.01 | 2.13 1.91 1.53 1.04 1.06 | 1.10 1.04 0.99 1.67 2.01 |
| 0.6        | 2.04 1.99 2.30 3.86 2.34 | 2.04 1.86 2.17 3.55 2.26 | 1.37 1.19 1.16 0.96 0.99 | 2.17 2.02 1.40 1.13 0.96 | 1.10 1.08 0.98 1.53 1.94 |
| 0.7        | 2.12 2.21 2.45 3.77 2.52 | 2.13 2.15 2.36 3.60 2.42 | 1.68 1.28 1.09 0.93 1.00 | 2.33 2.00 1.28 1.09 0.99 | 1.03 1.73 0.98 1.57 1.89 |
| 0.8        | 2.25 2.50 2.58 3.62 2.43 | 2.25 2.35 2.48 3.86 2.43 | 1.67 1.55 1.02 1.13 1.14 | 2.17 2.02 1.38 1.28 1.28 | 1.00 1.22 0.96 1.26 1.71 |
| 0.9        | 2.28 2.57 2.69 3.66 2.50 | 2.25 2.46 2.56 3.50 2.41 | 1.47 1.86 1.13 0.79 1.16 | 1.82 2.14 1.45 1.19 1.30 | 0.84 0.89 0.91 0.91 1.42 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_32.txt-->
<!--clang-x64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.80 5.41 5.12 3.02 2.33 | 1.65 1.56 1.62 3.29 2.28 | 1.36 1.35 1.33 1.01 1.00 | 2.31 2.24 2.25 1.14 1.09 | 1.03 1.00 1.03 2.09 2.37 |
| 0.1        | 1.94 1.82 1.70 3.16 2.10 | 1.73 1.61 1.59 2.96 1.95 | 1.38 1.34 1.31 1.02 0.97 | 2.11 2.04 2.04 1.07 1.05 | 1.01 0.99 0.99 1.82 2.22 |
| 0.2        | 1.86 1.83 1.82 3.33 2.31 | 1.80 1.63 1.76 3.23 2.22 | 1.38 1.31 1.29 0.98 0.98 | 2.19 1.88 1.77 1.07 1.02 | 1.01 0.97 0.99 1.77 2.10 |
| 0.3        | 2.03 2.00 1.95 3.32 2.41 | 1.84 1.72 1.90 3.23 2.23 | 1.32 1.29 1.24 1.02 0.99 | 2.17 1.70 1.61 1.05 0.99 | 0.99 0.96 0.96 1.69 1.95 |
| 0.4        | 2.21 2.12 2.31 3.55 2.40 | 2.03 1.80 2.06 3.29 2.31 | 1.29 1.23 1.17 0.98 1.03 | 2.25 1.71 1.44 0.97 0.93 | 0.99 0.96 0.98 1.59 1.82 |
| 0.5        | 2.21 2.04 2.32 3.52 2.42 | 2.07 1.78 2.18 3.25 2.36 | 1.35 1.15 1.08 1.01 1.03 | 2.19 1.76 1.34 1.06 0.97 | 1.00 0.99 0.98 1.55 1.77 |
| 0.6        | 2.45 2.26 2.60 3.44 2.49 | 2.15 1.96 2.31 3.32 2.29 | 1.49 1.18 1.06 0.96 1.06 | 2.23 1.97 1.40 1.20 1.13 | 1.02 1.07 0.99 1.36 1.67 |
| 0.7        | 2.57 2.66 2.75 3.53 2.55 | 2.24 2.26 2.68 3.41 2.44 | 1.64 1.31 1.04 1.12 1.06 | 2.38 2.27 1.47 1.24 1.25 | 0.96 1.66 0.99 1.13 1.60 |
| 0.8        | 2.60 2.54 2.89 3.57 2.49 | 2.33 2.27 2.57 3.44 2.39 | 1.68 1.74 1.06 1.14 1.12 | 2.27 2.33 1.63 2.10 1.27 | 1.00 1.13 0.95 0.97 1.46 |
| 0.9        | 2.67 2.71 2.89 3.69 2.50 | 2.40 2.45 2.69 3.17 2.47 | 1.48 1.84 1.31 0.91 1.09 | 1.81 2.40 1.87 1.21 1.44 | 0.85 0.81 0.91 0.85 1.16 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_64.txt-->
<!--clang-x64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 2.11 6.20 5.90 3.31 2.54 | 1.71 1.66 1.61 3.23 2.43 | 1.00 0.96 0.93 0.93 0.91 | 1.75 1.46 1.56 1.07 1.04 | 1.06 1.00 0.95 2.01 2.29 |
| 0.1        | 2.14 1.97 1.77 2.95 2.18 | 1.74 1.70 1.62 2.97 2.06 | 1.03 1.00 0.97 0.98 0.92 | 1.72 1.45 1.44 1.11 0.97 | 1.06 0.99 1.01 1.82 2.20 |
| 0.2        | 2.24 2.07 1.92 3.21 2.48 | 1.78 1.78 1.76 3.18 2.36 | 1.07 1.00 0.98 0.96 0.93 | 1.68 1.35 1.25 1.03 1.00 | 1.06 0.99 0.98 1.71 2.07 |
| 0.3        | 2.27 2.20 2.03 3.50 2.45 | 1.86 1.91 1.96 3.10 2.41 | 1.05 1.00 0.98 0.97 0.94 | 1.66 1.30 1.23 0.94 0.98 | 1.06 0.99 1.06 1.63 1.93 |
| 0.4        | 2.29 2.29 2.37 3.31 2.37 | 1.98 2.01 2.13 3.19 2.38 | 1.03 1.00 0.96 0.94 0.99 | 1.70 1.23 1.23 0.95 1.05 | 1.06 0.95 1.00 1.65 1.86 |
| 0.5        | 2.33 2.50 2.37 3.41 2.50 | 2.08 2.11 2.39 3.36 2.39 | 1.10 1.01 0.98 0.97 1.02 | 1.66 1.56 1.28 1.09 1.18 | 1.07 1.00 1.04 1.60 1.67 |
| 0.6        | 2.49 2.70 2.59 3.37 2.41 | 2.19 2.41 2.42 3.35 2.45 | 1.13 1.18 1.05 1.00 1.02 | 1.80 1.93 1.45 1.20 1.20 | 1.08 1.05 1.00 1.35 1.57 |
| 0.7        | 2.54 2.68 2.83 3.30 2.64 | 2.29 2.44 2.63 3.28 2.34 | 1.23 1.08 0.96 1.00 1.03 | 1.70 1.80 1.28 1.29 1.20 | 1.06 1.64 0.99 1.14 1.51 |
| 0.8        | 2.67 2.70 3.15 3.37 2.51 | 2.36 2.35 3.14 3.26 2.32 | 1.39 1.37 1.00 1.04 1.04 | 1.80 1.95 1.40 1.55 1.32 | 1.03 1.21 0.99 1.03 1.35 |
| 0.9        | 2.69 2.92 3.12 3.43 2.52 | 2.37 2.51 2.90 3.52 2.39 | 1.32 1.67 1.10 0.78 1.04 | 1.61 2.23 1.37 1.14 1.25 | 0.80 0.84 0.95 0.85 1.13 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_80.txt-->

### Clang 17, ARM64
<!--clang-arm64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.02 1.15 1.13 1.14 0.99 | 1.48 1.59 1.53 1.35 1.15 | 1.80 1.93 1.80 1.88 1.87 | 2.15 2.34 2.25 2.30 2.37 | 1.14 1.54 1.17 1.03 1.04 |
| 0.1        | 1.34 1.29 1.65 1.82 1.47 | 1.31 2.24 1.19 1.75 1.33 | 1.54 1.36 1.32 1.56 1.38 | 2.19 2.33 2.21 2.33 1.89 | 0.98 3.23 1.19 0.95 0.95 |
| 0.2        | 1.49 1.51 1.23 1.58 1.44 | 1.34 2.45 1.57 1.89 1.40 | 1.40 1.44 1.39 1.49 1.40 | 2.16 2.23 2.43 2.30 2.05 | 0.96 0.72 1.29 0.98 0.92 |
| 0.3        | 1.37 1.25 1.46 1.78 1.75 | 1.49 1.89 1.88 1.66 1.57 | 1.58 1.42 1.35 1.43 1.39 | 2.45 2.30 2.17 2.28 2.07 | 0.99 0.35 0.96 0.95 0.90 |
| 0.4        | 1.36 1.47 1.66 1.54 1.52 | 1.52 2.18 1.68 1.76 1.56 | 1.50 1.43 1.33 1.23 1.40 | 2.38 2.31 1.95 2.20 2.06 | 0.96 0.54 1.04 0.94 0.91 |
| 0.5        | 1.48 1.55 1.55 1.87 1.61 | 1.41 2.29 1.80 1.45 1.70 | 1.94 1.75 1.44 1.28 1.37 | 2.69 2.59 2.05 2.14 1.89 | 0.94 0.30 1.21 0.97 0.90 |
| 0.6        | 1.43 1.40 1.87 1.91 1.59 | 1.62 2.58 2.07 1.80 1.55 | 2.09 1.87 1.37 1.34 1.40 | 3.39 2.95 2.10 2.18 1.86 | 0.90 0.34 1.13 0.98 0.84 |
| 0.7        | 1.53 1.81 1.63 1.74 1.66 | 1.31 1.83 1.98 2.07 1.87 | 2.16 2.58 1.44 1.23 1.39 | 3.13 3.34 2.21 1.68 1.77 | 0.90 0.60 1.43 0.87 0.82 |
| 0.8        | 1.45 1.80 1.82 1.92 2.10 | 1.60 2.35 1.98 1.88 1.75 | 2.23 2.27 1.71 0.91 1.42 | 2.78 2.54 3.09 1.46 1.32 | 0.92 3.02 1.01 0.95 0.79 |
| 0.9        | 1.55 1.75 1.75 1.86 1.74 | 1.59 3.26 2.21 1.98 1.77 | 1.64 2.80 1.72 1.11 1.32 | 1.81 4.23 1.61 1.58 1.25 | 0.75 6.85 1.39 0.89 0.71 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_16.txt-->
<!--clang-arm64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.31 1.07 1.27 1.27 1.19 | 1.26 3.01 1.61 1.12 1.44 | 1.77 1.89 1.85 1.70 1.86 | 2.69 2.95 2.74 2.77 2.34 | 1.17 1.36 1.15 2.15 2.92 |
| 0.1        | 1.31 1.59 1.13 1.63 1.38 | 1.52 2.93 1.67 1.87 1.27 | 1.49 1.43 1.47 1.31 1.33 | 2.12 2.17 2.45 2.00 2.26 | 1.14 1.04 1.18 2.12 2.88 |
| 0.2        | 1.35 1.79 1.75 1.74 1.52 | 1.45 3.47 1.92 1.67 1.66 | 1.50 1.43 1.37 1.37 1.19 | 2.42 2.02 2.21 2.12 2.34 | 1.07 0.86 1.09 2.17 2.85 |
| 0.3        | 1.41 1.88 1.73 1.68 1.70 | 1.27 3.54 2.02 2.00 1.71 | 1.77 1.42 1.42 1.38 1.38 | 2.67 2.26 2.10 1.99 2.25 | 1.06 0.61 1.19 2.17 2.73 |
| 0.4        | 1.37 2.01 1.75 1.76 1.72 | 1.57 5.00 1.89 1.71 1.59 | 1.76 1.46 1.41 1.33 1.31 | 2.39 2.27 2.46 1.81 1.50 | 1.05 0.99 1.10 1.94 2.62 |
| 0.5        | 1.59 2.21 1.86 1.88 1.73 | 1.59 2.90 1.99 1.74 1.64 | 2.17 1.67 1.34 1.33 1.24 | 2.81 2.75 2.17 1.64 1.29 | 1.02 1.10 1.03 1.80 2.45 |
| 0.6        | 1.66 2.20 2.15 1.75 1.69 | 1.82 4.05 1.99 1.86 1.59 | 2.10 2.01 1.34 1.12 1.21 | 3.09 2.93 2.68 1.22 1.22 | 1.03 0.53 1.04 1.85 2.21 |
| 0.7        | 1.59 2.09 1.83 1.82 1.72 | 1.55 2.37 1.94 2.01 1.68 | 2.31 2.36 1.46 1.09 1.19 | 2.76 2.50 2.38 1.42 0.99 | 0.99 0.41 1.08 1.50 2.06 |
| 0.8        | 1.49 2.22 1.85 1.57 1.62 | 1.66 3.58 2.08 1.67 1.71 | 2.22 2.05 1.74 1.02 1.10 | 2.76 2.62 2.37 1.13 1.01 | 0.93 1.02 1.30 1.19 1.91 |
| 0.9        | 1.67 2.05 1.85 1.75 1.71 | 1.64 2.20 2.16 1.03 1.80 | 1.63 3.32 2.11 1.19 1.11 | 1.91 4.99 2.35 1.35 1.22 | 0.76 2.74 0.97 0.88 1.43 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_32.txt-->
<!--clang-arm64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.52 1.42 1.60 1.57 1.11 | 1.54 3.88 1.86 1.35 1.32 | 1.81 1.90 1.77 1.23 1.22 | 2.19 2.30 2.14 1.50 1.50 | 1.13 1.16 1.06 2.19 3.22 |
| 0.1        | 1.59 1.46 1.72 1.66 1.57 | 1.57 1.75 2.39 1.30 1.80 | 1.48 1.47 1.42 1.08 1.28 | 2.17 2.32 2.14 1.28 1.39 | 0.98 0.94 1.18 1.98 3.22 |
| 0.2        | 1.63 1.57 1.85 1.65 1.68 | 1.58 1.59 1.76 1.60 1.64 | 1.49 1.59 1.33 1.15 1.28 | 2.20 2.37 2.02 1.11 1.20 | 1.08 0.92 1.17 2.40 3.04 |
| 0.3        | 1.67 1.77 1.76 1.81 1.64 | 1.63 2.66 1.80 1.53 1.81 | 1.74 1.47 1.12 1.00 1.10 | 2.49 2.35 2.03 1.29 1.21 | 0.98 0.96 1.03 2.02 2.82 |
| 0.4        | 1.71 1.91 1.73 1.95 1.71 | 1.67 1.82 2.18 1.45 1.69 | 1.71 1.47 1.29 0.94 1.05 | 2.45 2.58 2.08 0.87 1.05 | 1.01 1.29 0.83 2.20 2.80 |
| 0.5        | 1.75 1.94 1.84 1.71 1.69 | 1.72 1.99 2.05 1.28 2.12 | 2.16 1.73 1.30 1.09 0.97 | 2.86 2.50 1.61 0.73 0.91 | 0.98 1.01 1.15 1.60 2.33 |
| 0.6        | 1.83 2.00 1.89 1.66 1.70 | 1.72 1.88 1.86 1.35 1.72 | 2.25 1.76 1.24 1.36 0.88 | 2.96 2.57 1.79 1.14 1.01 | 1.01 0.84 0.96 1.79 1.96 |
| 0.7        | 1.86 2.05 2.17 1.72 1.79 | 1.80 1.95 2.11 1.60 1.76 | 2.37 2.17 1.29 1.04 1.10 | 3.38 2.67 1.79 1.16 1.23 | 1.03 0.63 1.13 1.47 1.87 |
| 0.8        | 1.89 1.99 1.94 1.57 1.63 | 1.83 1.93 2.12 1.63 1.49 | 2.38 2.37 1.44 1.06 0.92 | 2.88 2.85 1.86 1.37 0.98 | 0.95 1.16 0.86 1.34 1.71 |
| 0.9        | 1.92 1.90 2.15 1.63 1.70 | 1.89 1.89 2.00 1.63 1.86 | 1.66 3.61 2.16 1.09 1.08 | 1.87 4.47 2.77 1.28 1.84 | 0.80 1.09 0.94 0.83 1.18 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_64.txt-->
<!--clang-arm64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.53 1.40 1.70 0.60 1.57 | 1.39 1.41 1.94 1.26 1.23 | 1.71 1.80 1.59 1.26 1.33 | 2.52 2.83 2.16 1.14 1.23 | 1.02 0.95 1.28 2.18 2.90 |
| 0.1        | 1.64 1.39 1.67 0.82 1.35 | 1.59 2.81 1.86 1.61 1.42 | 1.37 1.21 1.35 1.13 1.39 | 2.02 2.45 1.79 0.99 1.11 | 1.03 0.99 1.05 2.01 2.72 |
| 0.2        | 1.69 1.40 2.15 1.00 1.34 | 1.70 2.04 2.09 1.36 1.49 | 1.43 1.30 1.28 1.01 0.97 | 2.20 2.37 1.61 1.05 1.05 | 1.03 0.66 1.03 1.83 2.61 |
| 0.3        | 1.70 1.46 2.24 1.12 1.68 | 1.60 3.88 1.98 1.68 1.69 | 1.53 1.32 1.25 1.08 1.07 | 2.58 2.57 1.67 0.97 1.00 | 1.08 0.83 1.31 1.83 2.48 |
| 0.4        | 1.61 1.60 2.12 1.48 1.71 | 1.69 1.57 1.82 1.81 1.69 | 1.54 1.37 1.20 0.84 1.11 | 2.72 2.26 1.55 1.00 1.00 | 1.03 0.79 1.02 1.78 2.37 |
| 0.5        | 1.85 1.75 1.96 1.95 1.66 | 1.73 1.66 2.01 1.61 1.52 | 1.90 1.53 1.28 1.01 1.10 | 2.88 2.55 1.57 1.03 1.07 | 1.02 0.71 0.93 1.74 2.23 |
| 0.6        | 1.85 1.87 1.97 1.70 1.50 | 1.70 2.75 2.07 1.61 1.67 | 2.15 1.66 1.27 1.04 1.09 | 2.76 2.51 1.84 0.91 1.07 | 1.17 0.78 0.86 1.62 2.07 |
| 0.7        | 2.04 1.97 2.07 1.70 1.47 | 1.83 1.71 2.11 1.26 1.60 | 2.19 1.86 1.38 1.08 0.99 | 2.74 2.43 1.57 1.09 1.03 | 0.97 2.29 1.30 1.41 1.86 |
| 0.8        | 2.05 2.23 2.20 1.41 1.55 | 1.81 2.88 2.15 1.47 1.65 | 2.22 2.50 1.63 0.95 1.30 | 2.53 2.89 2.82 1.38 1.12 | 1.12 0.94 0.97 1.28 1.58 |
| 0.9        | 1.93 2.12 1.78 1.31 1.51 | 1.80 2.07 2.40 1.42 1.57 | 1.80 3.07 2.30 1.27 1.05 | 1.92 4.22 2.46 1.37 1.17 | 0.82 5.59 1.01 0.93 1.29 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_80.txt-->

### VS 2022, x64
<!--vs-x64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.86 0.94 0.92 1.13 1.18 | 0.99 0.87 0.99 1.12 1.13 | 1.09 1.08 1.08 0.96 0.79 | 1.63 1.74 1.69 1.54 1.00 | 1.00 0.98 0.99 0.99 0.98 |
| 0.1        | 0.90 0.88 0.99 1.12 1.33 | 1.04 1.00 1.06 1.23 1.20 | 1.12 1.11 1.09 0.76 0.76 | 1.55 1.62 1.49 0.10 1.17 | 1.00 0.99 0.98 0.96 0.94 |
| 0.2        | 0.93 0.92 1.08 1.27 1.44 | 1.02 0.99 1.13 1.24 1.43 | 1.11 1.08 1.07 0.98 0.73 | 1.60 1.58 1.48 1.44 0.88 | 0.99 0.99 0.98 0.96 0.94 |
| 0.3        | 0.97 1.00 1.18 1.26 1.57 | 1.04 1.06 1.23 1.40 1.54 | 1.08 1.09 1.05 0.95 0.70 | 1.56 1.57 1.48 1.43 0.85 | 0.99 0.99 0.98 0.95 0.93 |
| 0.4        | 1.02 1.07 1.27 1.49 1.66 | 1.10 1.11 1.33 1.50 1.54 | 1.06 1.07 1.04 0.80 0.57 | 1.59 1.58 1.46 1.42 0.75 | 0.98 0.99 0.98 0.96 0.93 |
| 0.5        | 1.08 1.20 1.36 1.73 1.92 | 1.13 1.27 1.41 1.62 1.87 | 1.11 1.04 1.03 0.99 0.64 | 1.58 1.61 1.41 1.35 0.73 | 0.99 0.99 0.98 0.95 0.91 |
| 0.6        | 1.13 1.38 1.44 1.71 2.07 | 1.21 1.42 1.48 1.78 2.01 | 1.15 1.05 0.99 0.94 0.60 | 1.55 1.70 1.39 1.00 0.68 | 0.99 1.00 0.97 0.92 0.89 |
| 0.7        | 1.19 1.43 1.53 1.81 2.13 | 1.27 1.77 1.60 1.86 1.93 | 1.20 1.04 0.93 0.74 0.57 | 1.48 1.68 1.34 1.13 0.59 | 0.98 1.00 0.96 0.94 0.87 |
| 0.8        | 1.23 1.47 1.59 1.85 2.21 | 1.31 1.48 1.63 2.06 1.97 | 1.18 1.16 0.89 0.63 0.56 | 1.41 1.67 1.29 1.05 0.61 | 0.94 1.10 0.94 0.92 0.85 |
| 0.9        | 1.29 1.45 1.65 1.96 2.31 | 1.33 1.66 1.80 2.06 2.07 | 1.04 1.55 1.02 0.80 0.66 | 1.12 1.64 1.15 1.06 0.70 | 0.85 1.14 0.92 0.90 0.82 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_16.txt-->
<!--vs-x64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.89 0.85 1.04 1.06 1.12 | 0.93 0.86 0.95 0.85 1.05 | 1.17 1.12 1.11 0.95 0.77 | 1.67 1.75 1.70 1.24 0.89 | 1.03 1.00 0.99 1.17 1.69 |
| 0.1        | 0.92 0.89 0.99 1.17 1.34 | 0.96 0.90 1.01 1.08 1.27 | 1.13 1.11 1.09 0.93 0.77 | 1.62 1.63 1.54 1.15 0.84 | 0.98 0.98 0.98 1.14 1.65 |
| 0.2        | 0.97 0.93 1.08 1.30 1.43 | 0.99 0.95 1.08 1.26 1.41 | 1.14 1.12 1.08 0.91 0.74 | 1.63 1.59 1.52 1.07 0.79 | 0.99 0.98 0.98 1.10 1.65 |
| 0.3        | 1.02 1.00 1.18 1.43 1.60 | 1.03 1.01 1.18 1.44 1.57 | 1.14 1.10 1.08 0.91 0.72 | 1.63 1.58 1.48 1.03 0.76 | 1.01 0.99 0.97 1.07 1.63 |
| 0.4        | 1.05 1.10 1.28 1.62 1.82 | 1.08 1.12 1.28 1.56 1.70 | 1.11 1.10 1.05 0.87 0.70 | 1.68 1.65 1.45 1.02 0.73 | 0.99 0.98 0.98 1.02 1.56 |
| 0.5        | 1.13 1.24 1.37 1.83 1.88 | 1.15 1.24 1.36 1.65 1.87 | 1.11 1.08 1.02 0.86 0.67 | 1.68 1.66 1.38 0.97 0.72 | 1.03 1.00 0.98 1.01 1.53 |
| 0.6        | 1.16 1.39 1.47 1.94 2.04 | 1.17 1.38 1.45 1.68 1.95 | 1.19 1.04 0.96 0.82 0.68 | 1.68 1.69 1.31 0.93 0.70 | 1.02 1.00 0.97 0.97 1.47 |
| 0.7        | 1.24 1.45 1.54 2.07 2.17 | 1.24 1.44 1.53 1.83 1.98 | 1.25 1.05 0.89 0.82 0.70 | 1.52 1.76 1.22 0.91 0.72 | 0.96 1.02 0.97 0.93 1.42 |
| 0.8        | 1.27 1.46 1.60 1.95 2.15 | 1.29 1.47 1.60 1.81 2.07 | 1.19 1.22 0.92 0.87 0.79 | 1.40 1.75 1.14 1.15 0.77 | 0.96 1.21 0.96 0.87 1.26 |
| 0.9        | 1.32 1.51 1.64 1.92 2.22 | 1.33 1.53 1.64 1.77 2.17 | 1.14 1.40 1.00 0.80 0.66 | 1.06 1.68 1.24 1.03 0.71 | 0.91 1.29 0.92 0.90 1.00 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_32.txt-->
<!--vs-x64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.87 0.81 1.06 0.97 0.94 | 0.95 0.88 0.93 0.90 1.01 | 1.16 1.12 1.10 0.75 0.73 | 1.72 1.65 1.55 0.75 0.74 | 1.04 1.00 0.98 1.21 1.63 |
| 0.1        | 0.87 0.85 0.93 1.13 1.23 | 0.98 0.91 0.95 1.04 1.17 | 1.18 1.12 1.08 0.74 0.73 | 1.66 1.53 1.41 0.72 0.71 | 1.01 0.98 0.97 1.18 1.58 |
| 0.2        | 0.92 0.89 1.00 1.35 1.37 | 0.98 0.94 1.02 1.27 1.45 | 1.16 1.09 1.05 0.72 0.72 | 1.67 1.46 1.32 0.72 0.70 | 1.01 0.99 0.97 1.13 1.56 |
| 0.3        | 0.96 0.97 1.10 1.54 1.62 | 1.02 1.01 1.10 1.44 1.50 | 1.15 1.04 0.98 0.72 0.71 | 1.68 1.38 1.25 0.69 0.69 | 1.00 1.00 0.97 1.13 1.56 |
| 0.4        | 1.02 1.05 1.19 1.65 1.64 | 1.07 1.09 1.19 1.55 1.68 | 1.09 1.00 0.93 0.71 0.72 | 1.69 1.34 1.12 0.68 0.69 | 1.04 0.99 0.97 1.04 1.48 |
| 0.5        | 1.11 1.16 1.29 1.75 1.75 | 1.13 1.21 1.28 1.66 1.63 | 1.15 0.95 0.84 0.74 0.76 | 1.72 1.30 1.08 0.68 0.72 | 1.03 1.00 0.97 1.05 1.47 |
| 0.6        | 1.14 1.30 1.37 1.82 1.87 | 1.25 1.34 1.37 1.72 1.88 | 1.16 0.92 0.86 0.81 0.83 | 1.70 1.37 1.05 0.74 0.78 | 1.05 1.03 0.98 0.99 1.41 |
| 0.7        | 1.20 1.39 1.44 1.88 2.01 | 1.23 1.42 1.43 1.76 1.85 | 1.25 1.03 0.86 0.85 0.82 | 1.54 1.54 1.12 0.86 0.76 | 1.04 1.08 0.98 0.92 1.38 |
| 0.8        | 1.22 1.46 1.49 1.94 2.10 | 1.31 1.45 1.48 1.84 1.93 | 1.23 1.22 0.85 0.86 0.78 | 1.43 1.72 1.12 1.01 0.74 | 1.02 1.28 0.96 0.83 1.23 |
| 0.9        | 1.25 1.44 1.54 1.97 2.14 | 1.31 1.47 1.53 1.82 2.02 | 1.04 1.36 0.92 0.79 0.70 | 1.07 1.69 1.03 0.94 0.71 | 0.98 1.46 0.94 0.81 1.10 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_64.txt-->
<!--vs-x64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.91 0.83 1.37 0.91 0.85 | 0.95 0.97 1.02 0.87 0.89 | 1.23 1.21 1.14 0.76 0.75 | 1.52 1.47 1.34 0.74 0.74 | 1.03 1.05 1.00 1.20 1.54 |
| 0.1        | 0.92 0.89 0.98 1.08 1.08 | 0.97 1.00 1.05 1.03 1.04 | 1.24 1.21 1.15 0.77 0.74 | 1.47 1.37 1.23 0.72 0.72 | 1.01 1.03 0.98 1.19 1.47 |
| 0.2        | 0.96 0.94 1.05 1.39 1.27 | 1.00 1.05 1.11 1.28 1.04 | 1.21 1.19 1.11 0.76 0.73 | 1.48 1.32 1.17 0.70 0.69 | 1.01 1.06 1.01 1.14 1.48 |
| 0.3        | 0.98 1.02 1.16 1.54 1.32 | 1.04 1.12 1.21 1.43 1.32 | 1.22 1.16 1.06 0.70 0.74 | 1.46 1.24 1.08 0.68 0.68 | 1.01 1.06 0.98 1.11 1.45 |
| 0.4        | 1.05 1.09 1.24 1.65 1.46 | 1.07 1.18 1.27 1.53 1.32 | 1.16 1.13 1.02 0.80 0.78 | 1.49 1.19 1.00 0.67 0.72 | 1.02 1.08 0.99 1.05 1.36 |
| 0.5        | 1.15 1.23 1.33 1.73 1.59 | 1.14 1.29 1.34 1.61 1.53 | 1.18 1.15 1.03 0.84 0.86 | 1.50 1.19 1.01 0.53 0.61 | 1.03 1.12 0.96 1.06 1.34 |
| 0.6        | 1.15 1.37 1.41 1.81 1.69 | 1.19 1.43 1.43 1.69 1.50 | 1.22 1.23 1.02 0.80 0.74 | 1.19 1.11 0.99 0.79 0.75 | 1.04 1.15 1.02 1.01 1.28 |
| 0.7        | 1.24 1.44 1.47 1.85 1.75 | 1.26 1.48 1.48 1.74 1.51 | 1.25 1.16 1.00 0.87 0.77 | 1.61 1.48 1.10 0.81 0.66 | 1.03 1.27 1.03 0.93 1.21 |
| 0.8        | 1.27 1.50 1.53 1.89 1.85 | 1.30 1.53 1.52 1.77 1.64 | 1.23 1.23 0.97 0.78 0.74 | 1.30 1.40 0.98 0.98 0.66 | 1.04 1.70 1.05 0.87 1.09 |
| 0.9        | 1.35 1.51 1.56 1.93 1.92 | 1.32 1.55 1.57 1.80 1.62 | 1.04 1.30 0.95 0.86 0.61 | 1.04 1.46 0.86 0.87 0.65 | 0.96 2.43 1.13 0.99 0.92 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_80.txt-->

### GCC 15, x86
<!--gcc-x86/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.06 1.22 1.37 1.39 1.23 | 1.03 0.99 1.02 1.06 1.29 | 0.62 0.61 0.61 0.61 0.61 | 1.18 1.29 1.30 1.30 1.25 | 0.93 0.99 0.98 1.00 1.80 |
| 0.1        | 1.04 1.00 1.12 1.32 1.25 | 1.03 0.98 1.02 1.11 1.23 | 0.57 0.56 0.56 0.55 0.58 | 1.12 1.15 1.15 1.15 1.13 | 0.91 0.99 0.99 1.08 1.75 |
| 0.2        | 1.03 0.99 1.15 1.46 1.32 | 1.02 0.96 1.06 1.09 1.38 | 0.56 0.55 0.55 0.54 0.57 | 1.15 1.12 1.11 1.12 1.07 | 0.91 0.98 0.97 0.95 1.65 |
| 0.3        | 1.04 0.99 1.19 1.49 1.37 | 1.02 0.97 1.10 1.14 1.62 | 0.58 0.55 0.54 0.54 0.57 | 1.17 1.11 1.09 1.08 1.04 | 0.91 0.98 0.96 0.88 1.58 |
| 0.4        | 1.06 1.03 1.17 1.39 1.41 | 1.04 1.01 1.14 1.24 1.68 | 0.60 0.54 0.53 0.53 0.57 | 1.21 1.14 1.06 1.05 0.99 | 0.89 0.99 0.97 0.88 1.54 |
| 0.5        | 1.06 1.07 1.20 1.43 1.60 | 1.04 1.04 1.18 1.25 1.76 | 0.59 0.54 0.52 0.52 0.57 | 1.20 1.23 1.03 1.02 0.95 | 0.91 0.99 0.96 0.87 1.42 |
| 0.6        | 1.07 1.19 1.23 1.36 1.73 | 1.05 1.10 1.21 1.30 1.72 | 0.65 0.56 0.51 0.50 0.59 | 1.23 1.32 1.03 0.96 0.86 | 0.88 1.03 0.95 0.83 1.39 |
| 0.7        | 1.08 1.16 1.26 1.40 1.77 | 1.06 1.16 1.24 1.34 1.77 | 0.67 0.65 0.51 0.49 0.62 | 1.23 1.35 1.06 0.90 0.81 | 0.84 1.56 0.94 0.85 1.24 |
| 0.8        | 1.09 1.18 1.29 1.42 1.95 | 1.08 1.16 1.26 1.42 1.74 | 0.70 0.73 0.57 0.47 0.64 | 1.22 1.38 1.15 0.82 0.75 | 0.84 1.04 0.92 0.91 1.04 |
| 0.9        | 1.10 1.19 1.29 1.44 1.94 | 1.09 1.17 1.26 1.36 1.72 | 0.69 0.81 0.73 0.42 0.59 | 1.03 1.42 1.03 0.80 0.64 | 0.71 0.73 0.86 0.78 0.85 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_16.txt-->
<!--gcc-x86/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.06 1.40 1.53 1.40 1.22 | 1.04 1.00 1.03 1.06 1.25 | 0.62 0.61 0.60 0.63 0.65 | 1.20 1.31 1.30 1.28 1.04 | 1.02 1.00 0.98 0.91 1.49 |
| 0.1        | 1.06 1.03 1.15 1.41 1.21 | 1.04 1.00 1.04 1.16 1.31 | 0.59 0.56 0.55 0.58 0.62 | 1.16 1.18 1.15 1.17 0.94 | 1.00 1.00 0.98 0.88 1.42 |
| 0.2        | 1.06 1.02 1.17 1.50 1.37 | 1.05 0.99 1.06 1.25 1.72 | 0.59 0.55 0.55 0.59 0.62 | 1.21 1.14 1.11 1.12 0.88 | 1.01 0.98 0.97 0.86 1.37 |
| 0.3        | 1.06 1.02 1.21 1.68 1.41 | 1.05 0.99 1.10 1.35 1.66 | 0.60 0.55 0.55 0.58 0.64 | 1.20 1.15 1.12 1.05 0.83 | 1.00 0.98 0.97 0.82 1.34 |
| 0.4        | 1.07 1.07 1.25 1.81 1.50 | 1.04 1.04 1.14 1.34 1.71 | 0.61 0.55 0.54 0.59 0.66 | 1.22 1.17 1.09 1.03 0.79 | 1.01 0.98 0.97 0.79 1.30 |
| 0.5        | 1.07 1.20 1.21 1.66 1.55 | 1.05 1.16 1.18 1.53 1.90 | 0.62 0.56 0.54 0.63 0.70 | 1.26 1.22 1.07 1.01 0.78 | 1.01 0.98 0.95 0.75 1.20 |
| 0.6        | 1.08 1.22 1.25 1.73 1.60 | 1.05 1.18 1.21 1.48 1.96 | 0.70 0.59 0.54 0.63 0.72 | 1.30 1.29 1.07 0.99 0.84 | 1.01 1.01 0.94 0.69 1.16 |
| 0.7        | 1.09 1.22 1.28 1.76 1.59 | 1.07 1.16 1.23 1.57 1.75 | 0.69 0.68 0.55 0.66 0.74 | 1.28 1.31 1.03 0.98 0.87 | 0.99 1.44 0.94 0.67 1.05 |
| 0.8        | 1.10 1.22 1.30 1.86 1.72 | 1.07 1.17 1.25 1.70 2.01 | 0.70 0.75 0.63 0.55 0.63 | 1.21 1.32 0.98 0.80 0.73 | 0.96 1.07 0.91 0.90 0.95 |
| 0.9        | 1.11 1.21 1.31 1.83 1.52 | 1.08 1.17 1.26 1.63 1.80 | 0.72 0.81 0.72 0.43 0.45 | 1.06 1.40 1.00 0.68 0.56 | 0.84 0.76 0.85 0.84 0.77 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_32.txt-->
<!--gcc-x86/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.01 1.03 1.60 1.51 1.42 | 1.00 0.97 1.00 1.04 1.43 | 0.64 0.63 0.61 0.67 0.76 | 1.26 1.30 1.28 0.86 0.86 | 0.99 0.99 0.98 0.57 0.76 |
| 0.1        | 1.01 1.00 1.08 1.54 1.42 | 1.00 0.98 1.01 1.02 1.46 | 0.61 0.60 0.58 0.65 0.75 | 1.24 1.20 1.20 0.83 0.84 | 0.99 0.98 0.98 0.54 0.75 |
| 0.2        | 1.02 1.02 1.10 1.60 1.52 | 1.00 0.99 1.02 1.08 1.54 | 0.61 0.59 0.58 0.75 0.77 | 1.18 1.19 1.18 0.81 0.84 | 0.99 0.98 0.97 0.53 0.70 |
| 0.3        | 1.02 1.05 1.13 1.68 1.61 | 1.00 1.03 1.05 1.19 1.62 | 0.63 0.59 0.58 0.71 0.79 | 1.18 1.17 1.11 0.80 0.84 | 0.99 0.97 0.97 0.51 0.66 |
| 0.4        | 1.03 1.10 1.15 1.79 1.71 | 1.01 1.07 1.08 1.30 1.71 | 0.65 0.60 0.61 0.69 0.78 | 1.31 1.13 1.02 0.78 0.83 | 0.99 0.97 0.97 0.53 0.67 |
| 0.5        | 1.04 1.15 1.22 1.87 1.78 | 1.02 1.11 1.11 1.40 1.87 | 0.66 0.65 0.70 0.69 0.78 | 1.23 1.07 1.01 0.78 0.84 | 0.98 0.97 0.96 0.49 0.64 |
| 0.6        | 1.06 1.16 1.24 1.94 1.85 | 1.02 1.13 1.13 1.47 1.93 | 0.73 0.71 0.77 0.64 0.72 | 1.28 1.12 0.97 0.72 0.84 | 0.98 0.98 0.95 0.44 0.61 |
| 0.7        | 1.08 1.17 1.27 1.97 1.91 | 1.03 1.13 1.16 1.53 1.98 | 0.73 0.75 0.71 0.53 0.56 | 1.29 1.16 0.96 0.68 0.68 | 0.96 1.21 0.94 0.41 0.57 |
| 0.8        | 1.10 1.17 1.22 1.63 1.90 | 1.04 1.13 1.18 1.60 2.04 | 0.72 0.74 0.63 0.47 0.48 | 1.23 1.39 1.00 0.86 0.61 | 0.94 0.98 0.92 0.91 0.53 |
| 0.9        | 1.10 1.17 1.25 1.62 1.55 | 1.05 1.13 1.19 1.60 1.58 | 0.71 0.82 0.72 0.44 0.47 | 1.02 1.42 1.02 0.67 0.59 | 0.84 0.81 0.86 0.83 0.49 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_64.txt-->
<!--gcc-x86/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.98 0.99 1.52 1.45 1.39 | 0.97 0.95 1.07 1.51 1.46 | 0.70 0.67 0.66 0.73 0.80 | 1.18 1.25 1.23 0.85 0.89 | 1.05 1.02 1.00 0.44 0.54 |
| 0.1        | 0.98 0.97 1.03 1.48 1.40 | 0.97 0.96 1.07 1.53 1.42 | 0.66 0.63 0.62 0.70 0.81 | 1.12 1.10 1.10 0.84 0.89 | 1.05 1.02 0.99 0.40 0.48 |
| 0.2        | 0.98 0.97 1.05 1.51 1.46 | 0.97 0.96 0.99 1.13 1.46 | 0.66 0.64 0.62 0.83 0.83 | 1.17 1.11 1.07 0.83 0.86 | 1.06 1.01 0.99 0.38 0.47 |
| 0.3        | 0.98 0.99 1.07 1.59 1.53 | 0.97 0.98 1.02 1.29 1.59 | 0.68 0.64 0.62 0.68 0.80 | 1.20 1.04 1.05 0.83 0.87 | 1.05 1.01 0.98 0.37 0.47 |
| 0.4        | 0.99 1.03 1.09 1.64 1.57 | 0.98 1.02 1.04 1.28 1.63 | 0.68 0.66 0.62 0.80 0.80 | 1.23 1.05 0.92 0.82 0.87 | 1.05 1.01 0.99 0.37 0.46 |
| 0.5        | 0.99 1.06 1.09 1.69 1.67 | 0.99 1.05 1.07 1.35 1.74 | 0.70 0.73 0.78 0.76 0.76 | 1.23 0.95 0.94 0.83 0.85 | 1.04 1.02 0.98 0.36 0.44 |
| 0.6        | 1.00 1.07 1.13 1.72 1.70 | 0.99 1.06 1.09 1.43 1.78 | 0.78 0.72 0.68 0.55 0.64 | 1.27 1.14 0.93 0.72 0.79 | 1.05 1.02 0.97 0.34 0.44 |
| 0.7        | 1.00 1.08 1.15 1.74 1.72 | 1.00 1.07 1.10 1.43 1.82 | 0.73 0.74 0.60 0.56 0.55 | 1.19 1.20 0.96 0.80 0.71 | 1.02 1.24 0.97 0.31 0.40 |
| 0.8        | 1.01 1.07 1.19 1.73 1.78 | 1.01 1.06 1.11 1.48 1.87 | 0.74 0.78 0.59 0.66 0.51 | 1.20 1.36 0.95 0.69 0.67 | 1.01 1.04 0.96 0.92 0.37 |
| 0.9        | 1.01 1.07 1.19 1.51 1.48 | 1.01 1.07 1.12 1.47 1.43 | 0.73 0.85 0.69 0.43 0.51 | 1.02 1.42 0.96 0.63 0.63 | 0.93 0.90 0.91 0.82 0.35 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_80.txt-->

### Clang 20, x86
<!--clang-x86/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.11 1.29 1.44 1.33 1.19 | 1.15 1.09 1.11 1.12 1.30 | 0.55 0.54 0.54 0.54 0.55 | 0.98 1.08 1.07 1.07 1.04 | 1.02 1.03 1.01 1.16 2.08 |
| 0.1        | 1.14 1.10 1.20 1.46 1.30 | 1.18 1.12 1.14 1.24 1.34 | 0.56 0.55 0.54 0.54 0.56 | 0.96 1.00 0.98 0.98 0.96 | 0.97 1.01 1.01 1.43 2.14 |
| 0.2        | 1.17 1.13 1.24 1.41 1.38 | 1.20 1.14 1.18 1.26 1.46 | 0.56 0.54 0.54 0.53 0.57 | 0.93 0.98 0.96 0.97 0.95 | 0.94 1.01 0.99 1.48 1.96 |
| 0.3        | 1.20 1.15 1.23 1.28 1.45 | 1.22 1.17 1.24 1.26 1.51 | 0.56 0.54 0.53 0.53 0.57 | 0.96 0.97 0.95 0.95 0.92 | 0.96 1.01 0.99 1.52 1.96 |
| 0.4        | 1.22 1.12 1.30 1.34 1.53 | 1.25 1.17 1.30 1.27 1.63 | 0.56 0.54 0.53 0.53 0.58 | 0.99 1.00 0.94 0.93 0.91 | 0.97 1.03 0.98 1.40 1.96 |
| 0.5        | 1.26 1.17 1.35 1.55 1.56 | 1.28 1.24 1.35 1.44 1.67 | 0.56 0.54 0.52 0.52 0.60 | 0.98 1.04 0.92 0.91 0.86 | 0.96 1.04 0.98 1.06 1.86 |
| 0.6        | 1.28 1.28 1.40 1.38 1.67 | 1.32 1.29 1.40 1.44 1.67 | 0.59 0.53 0.51 0.52 0.63 | 1.00 1.08 0.92 0.87 0.85 | 0.92 1.11 0.97 0.94 1.64 |
| 0.7        | 1.30 1.28 1.45 1.40 1.65 | 1.33 1.26 1.45 1.40 1.74 | 0.64 0.56 0.50 0.51 0.70 | 1.02 1.12 0.95 0.84 0.80 | 0.90 1.82 0.96 0.91 1.49 |
| 0.8        | 1.35 1.36 1.49 1.44 1.69 | 1.38 1.29 1.49 1.46 1.66 | 0.67 0.62 0.51 0.50 0.70 | 1.02 1.15 1.05 0.79 0.78 | 0.85 1.15 0.93 0.99 1.38 |
| 0.9        | 1.36 1.39 1.52 1.49 1.58 | 1.37 1.39 1.52 1.45 1.16 | 0.66 0.71 0.76 0.57 0.64 | 0.94 1.30 1.01 0.85 0.67 | 0.76 0.80 0.88 0.80 0.99 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_16.txt-->
<!--clang-x86/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.19 1.54 1.70 1.66 1.28 | 1.18 1.10 1.14 1.13 1.52 | 0.56 0.54 0.54 0.58 0.63 | 1.03 1.07 1.07 1.00 0.89 | 1.00 1.02 1.01 1.15 1.86 |
| 0.1        | 1.22 1.14 1.28 1.63 1.24 | 1.22 1.12 1.17 1.18 1.47 | 0.57 0.54 0.54 0.62 0.67 | 0.99 0.98 0.96 0.92 0.86 | 0.95 1.00 1.00 1.19 1.79 |
| 0.2        | 1.26 1.16 1.32 1.61 1.38 | 1.26 1.15 1.22 1.34 1.45 | 0.57 0.54 0.53 0.68 0.67 | 0.99 0.96 0.95 0.88 0.83 | 0.96 1.00 0.99 1.28 1.79 |
| 0.3        | 1.28 1.18 1.39 1.73 1.49 | 1.28 1.16 1.28 1.38 1.50 | 0.56 0.53 0.53 0.65 0.71 | 1.00 0.96 0.93 0.86 0.83 | 0.96 1.00 0.99 1.23 1.77 |
| 0.4        | 1.30 1.18 1.44 1.89 1.46 | 1.30 1.17 1.34 1.65 1.55 | 0.55 0.53 0.53 0.68 0.74 | 1.05 0.98 0.92 0.88 0.82 | 0.97 1.00 0.99 1.03 1.49 |
| 0.5        | 1.34 1.19 1.43 1.73 1.48 | 1.33 1.17 1.40 1.65 1.52 | 0.58 0.53 0.52 0.70 0.73 | 1.05 1.00 0.90 0.89 0.82 | 0.95 1.01 0.98 0.99 1.50 |
| 0.6        | 1.38 1.34 1.50 1.49 1.61 | 1.36 1.25 1.48 1.70 1.33 | 0.60 0.53 0.52 0.69 0.73 | 1.05 1.04 0.90 0.84 0.82 | 0.94 1.09 0.97 1.05 1.47 |
| 0.7        | 1.42 1.36 1.55 1.53 1.66 | 1.39 1.40 1.52 1.66 1.21 | 0.65 0.55 0.59 0.74 0.74 | 1.11 1.09 1.00 0.82 0.87 | 0.93 1.63 0.95 0.94 1.30 |
| 0.8        | 1.44 1.37 1.59 1.57 1.72 | 1.43 1.40 1.54 1.73 1.30 | 0.63 0.60 0.68 0.56 0.67 | 1.05 1.11 0.93 0.74 0.77 | 0.91 1.12 0.94 0.95 1.13 |
| 0.9        | 1.44 1.46 1.62 1.55 1.59 | 1.45 1.41 1.58 1.64 1.28 | 0.62 0.67 0.63 0.50 0.46 | 0.96 1.23 0.86 0.71 0.58 | 0.76 0.74 0.87 0.84 0.88 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_32.txt-->
<!--clang-x86/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.57 1.32 2.48 2.34 1.45 | 1.41 1.32 1.35 1.37 1.62 | 0.57 0.54 0.54 0.68 0.76 | 1.08 1.14 1.13 0.83 0.86 | 0.91 0.97 0.99 0.81 0.99 |
| 0.1        | 1.63 1.52 1.62 2.10 1.52 | 1.47 1.37 1.38 1.48 1.81 | 0.58 0.55 0.54 0.73 0.77 | 1.02 1.04 1.03 0.82 0.88 | 0.95 0.98 0.98 0.76 0.92 |
| 0.2        | 1.68 1.57 1.65 1.92 1.54 | 1.53 1.42 1.45 1.54 1.67 | 0.58 0.55 0.54 0.80 0.80 | 1.02 1.01 0.99 0.84 0.86 | 0.95 0.97 1.02 0.72 0.88 |
| 0.3        | 1.72 1.62 1.75 2.02 1.61 | 1.56 1.46 1.52 1.61 1.53 | 0.57 0.55 0.54 0.79 0.80 | 1.05 0.98 0.98 0.84 0.95 | 0.95 0.97 0.98 0.62 0.78 |
| 0.4        | 1.76 1.66 1.79 2.10 1.71 | 1.61 1.51 1.59 1.65 1.52 | 0.55 0.55 0.54 0.78 0.81 | 1.08 1.04 0.98 0.79 0.85 | 0.95 0.98 0.97 0.64 0.79 |
| 0.5        | 1.81 1.72 1.92 2.14 1.89 | 1.65 1.57 1.68 1.69 1.48 | 0.58 0.56 0.55 0.89 0.87 | 1.12 1.04 0.99 0.80 0.88 | 0.95 0.99 0.96 0.60 0.76 |
| 0.6        | 1.84 1.78 1.99 2.16 1.74 | 1.70 1.63 1.55 1.90 1.62 | 0.59 0.56 0.57 0.80 0.72 | 1.14 1.05 0.93 0.84 0.88 | 0.94 1.01 0.97 0.58 0.74 |
| 0.7        | 1.88 1.91 2.06 2.18 1.93 | 1.74 1.68 1.79 1.78 1.45 | 0.62 0.55 0.51 0.52 0.58 | 1.07 1.18 0.87 0.62 0.67 | 0.94 1.42 0.96 0.54 0.69 |
| 0.8        | 1.93 1.85 1.98 1.81 1.76 | 1.79 1.70 1.85 1.84 1.24 | 0.65 0.59 0.47 0.43 0.49 | 1.09 1.18 0.92 0.61 0.61 | 0.88 1.12 0.94 0.92 0.63 |
| 0.9        | 1.96 1.91 2.00 1.80 1.62 | 1.81 1.75 1.93 1.91 1.14 | 0.63 0.67 0.53 0.39 0.49 | 0.99 1.26 0.87 0.68 0.56 | 0.80 0.81 0.89 0.85 0.58 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_64.txt-->
<!--clang-x86/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 2.12 1.53 3.12 2.88 2.22 | 1.68 1.58 1.70 2.37 2.04 | 0.55 0.54 0.53 0.69 0.76 | 1.06 1.08 1.08 0.88 0.88 | 1.00 1.00 1.00 0.64 0.78 |
| 0.1        | 2.14 1.95 2.02 2.27 1.93 | 1.73 1.63 1.77 2.27 1.89 | 0.58 0.55 0.54 0.71 0.79 | 1.06 1.02 1.01 0.87 0.90 | 0.98 1.00 0.99 0.57 0.73 |
| 0.2        | 2.17 1.98 2.04 2.40 1.87 | 1.77 1.67 1.82 2.10 1.83 | 0.57 0.55 0.57 0.80 0.80 | 1.06 1.00 1.00 0.88 0.88 | 0.98 1.00 1.01 0.57 0.68 |
| 0.3        | 2.18 2.02 2.07 2.29 1.88 | 1.79 1.70 1.71 1.70 1.87 | 0.57 0.61 0.60 0.78 0.80 | 1.07 0.99 0.99 0.89 0.85 | 0.99 1.00 0.97 0.56 0.66 |
| 0.4        | 2.21 2.07 2.13 2.18 1.80 | 1.83 1.75 1.77 1.70 1.91 | 0.57 0.69 0.68 0.73 0.77 | 1.11 1.01 0.99 0.84 0.84 | 0.98 1.00 1.00 0.57 0.66 |
| 0.5        | 2.24 2.11 2.27 2.18 1.84 | 1.88 1.79 1.83 1.71 1.86 | 0.58 0.74 0.74 0.68 0.70 | 1.11 0.99 0.96 0.87 0.85 | 0.98 1.00 0.99 0.52 0.64 |
| 0.6        | 2.27 2.15 2.33 2.18 1.92 | 1.91 1.84 1.90 1.71 1.91 | 0.61 0.67 0.79 0.56 0.58 | 1.14 1.01 0.95 0.82 0.81 | 0.97 1.02 1.00 0.47 0.62 |
| 0.7        | 2.30 2.20 2.41 2.18 1.98 | 1.95 1.88 1.94 1.76 1.76 | 0.59 0.57 0.65 0.43 0.46 | 1.05 1.11 0.95 0.66 0.68 | 0.97 1.31 0.95 0.50 0.62 |
| 0.8        | 2.32 2.22 2.44 2.15 1.96 | 1.98 1.90 2.00 1.80 1.86 | 0.61 0.59 0.52 0.51 0.44 | 1.08 1.17 0.88 0.55 0.67 | 0.95 1.07 0.94 0.92 0.59 |
| 0.9        | 2.32 2.25 2.46 1.85 1.66 | 1.99 1.94 2.04 1.79 1.40 | 0.61 0.65 0.57 0.45 0.45 | 0.99 1.19 0.88 0.67 0.64 | 0.91 0.88 0.92 0.82 0.49 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_80.txt-->

### VS 2022, x86
<!--vs-x86/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.02 1.14 1.07 1.10 1.28 | 1.04 1.02 1.04 1.05 1.16 | 0.62 0.61 0.60 0.60 0.52 | 0.94 0.99 0.97 0.93 0.77 | 1.07 1.06 1.06 1.06 1.83 |
| 0.1        | 1.04 1.02 1.06 1.09 1.23 | 1.06 1.04 1.06 1.08 1.13 | 0.62 0.61 0.60 0.60 0.53 | 0.90 0.91 0.88 0.86 0.70 | 1.00 0.79 1.03 1.11 1.83 |
| 0.2        | 1.06 1.04 1.09 1.12 1.26 | 1.07 1.05 1.09 1.14 1.15 | 0.62 0.60 0.60 0.59 0.51 | 0.91 0.89 0.87 0.83 0.66 | 0.99 1.03 1.03 1.08 1.84 |
| 0.3        | 1.06 1.07 1.13 1.15 1.30 | 1.09 1.08 1.13 1.16 1.19 | 0.61 0.59 0.58 0.57 0.48 | 0.90 0.88 0.85 0.80 0.64 | 1.00 1.03 1.02 1.05 1.82 |
| 0.4        | 1.08 1.12 1.17 1.20 1.34 | 1.09 1.13 1.18 1.25 1.19 | 0.61 0.58 0.57 0.56 0.46 | 0.94 0.91 0.83 0.78 0.60 | 0.99 1.02 1.02 1.03 1.75 |
| 0.5        | 1.09 1.17 1.22 1.20 1.34 | 1.10 1.18 1.22 1.31 1.22 | 0.61 0.58 0.56 0.55 0.46 | 0.93 0.93 0.82 0.76 0.57 | 0.98 1.02 1.02 1.01 1.60 |
| 0.6        | 1.11 1.21 1.25 1.29 1.40 | 1.13 1.22 1.26 1.37 1.59 | 0.63 0.56 0.53 0.53 0.44 | 0.92 0.97 0.81 0.72 0.54 | 0.98 1.01 1.02 0.98 1.52 |
| 0.7        | 1.13 1.23 1.29 1.33 1.34 | 1.15 1.24 1.29 1.40 1.44 | 0.67 0.56 0.51 0.51 0.43 | 0.91 0.98 0.81 0.70 0.51 | 0.96 1.01 1.01 0.97 1.45 |
| 0.8        | 1.14 1.24 1.30 1.47 1.32 | 1.16 1.26 1.31 1.30 1.49 | 0.63 0.64 0.49 0.45 0.42 | 0.83 0.97 0.84 0.63 0.51 | 0.90 1.11 0.97 1.01 1.30 |
| 0.9        | 1.17 1.26 1.32 1.48 1.49 | 1.19 1.26 1.32 1.31 1.49 | 0.58 0.70 0.60 0.43 0.46 | 0.70 0.94 0.84 0.58 0.53 | 0.81 1.11 0.93 0.97 1.11 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_16.txt-->
<!--vs-x86/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.99 1.07 0.99 1.02 1.31 | 0.96 0.96 0.98 0.96 1.12 | 0.64 0.61 0.60 0.57 0.52 | 0.98 0.99 0.97 0.81 0.71 | 1.05 1.05 1.04 1.15 1.84 |
| 0.1        | 0.98 0.98 1.01 1.01 1.21 | 0.98 0.98 1.00 1.05 1.20 | 0.64 0.61 0.59 0.57 0.51 | 0.96 0.91 0.88 0.69 0.66 | 0.98 1.02 1.02 1.06 1.73 |
| 0.2        | 0.99 1.00 1.05 1.09 1.29 | 1.01 1.00 1.04 1.11 1.22 | 0.64 0.60 0.59 0.55 0.50 | 0.99 0.89 0.86 0.68 0.63 | 0.97 1.02 1.02 1.02 1.70 |
| 0.3        | 1.01 1.03 1.09 1.19 1.33 | 1.03 1.04 1.08 1.12 1.32 | 0.63 0.59 0.57 0.54 0.49 | 0.98 0.88 0.85 0.64 0.62 | 0.98 1.02 1.02 1.03 1.68 |
| 0.4        | 1.03 1.08 1.12 1.23 1.43 | 1.04 1.09 1.12 1.23 1.34 | 0.63 0.58 0.56 0.54 0.50 | 1.02 0.91 0.83 0.63 0.61 | 0.95 1.02 1.02 1.00 1.67 |
| 0.5        | 1.05 1.13 1.17 1.38 1.43 | 1.06 1.13 1.16 1.24 1.36 | 0.64 0.57 0.55 0.53 0.50 | 1.00 0.93 0.81 0.63 0.61 | 0.95 1.01 1.01 1.00 1.81 |
| 0.6        | 1.06 1.18 1.22 1.34 1.53 | 1.09 1.17 1.20 1.33 1.45 | 0.68 0.56 0.54 0.54 0.49 | 1.05 0.99 0.80 0.62 0.60 | 0.96 1.00 1.01 1.00 1.52 |
| 0.7        | 1.09 1.20 1.24 1.37 1.60 | 1.10 1.20 1.24 1.43 1.56 | 0.69 0.58 0.52 0.55 0.52 | 0.95 1.02 0.80 0.64 0.62 | 0.93 1.01 1.00 0.95 1.57 |
| 0.8        | 1.12 1.22 1.28 1.38 1.57 | 1.12 1.22 1.27 1.39 1.56 | 0.68 0.66 0.54 0.54 0.52 | 0.90 1.02 0.86 0.64 0.63 | 0.88 1.13 0.98 1.02 1.58 |
| 0.9        | 1.13 1.23 1.29 1.42 1.68 | 1.14 1.23 1.28 1.47 1.72 | 0.61 0.73 0.57 0.43 0.44 | 0.71 0.95 0.78 0.57 0.52 | 0.73 1.15 0.93 0.96 1.30 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_32.txt-->
<!--vs-x86/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.95 1.04 1.01 0.93 ---- | 0.96 1.06 1.23 1.11 ---- | 0.67 0.63 0.61 0.53 ---- | 0.97 0.97 0.93 0.64 ---- | 1.01 1.04 1.01 0.95 ---- |
| 0.1        | 0.97 1.06 1.22 1.01 ---- | 0.98 1.09 1.25 1.15 ---- | 0.67 0.64 0.61 0.52 ---- | 0.95 0.90 0.84 0.60 ---- | 0.94 1.03 0.99 0.95 ---- |
| 0.2        | 1.00 1.09 1.26 1.13 ---- | 1.01 1.11 1.27 1.15 ---- | 0.67 0.63 0.60 0.52 ---- | 0.97 0.88 0.82 0.59 ---- | 0.94 1.03 0.99 0.89 ---- |
| 0.3        | 1.03 1.13 1.29 1.24 ---- | 1.04 1.14 1.32 1.26 ---- | 0.67 0.62 0.59 0.51 ---- | 0.99 0.87 0.80 0.58 ---- | 0.94 1.03 0.99 0.92 ---- |
| 0.4        | 1.05 1.17 1.32 1.30 ---- | 1.06 1.18 1.33 1.29 ---- | 0.66 0.61 0.57 0.49 ---- | 0.99 0.91 0.77 0.55 ---- | 0.94 1.03 1.00 0.87 ---- |
| 0.5        | 1.08 1.21 1.42 1.37 ---- | 1.09 1.22 1.38 1.34 ---- | 0.69 0.61 0.56 0.48 ---- | 1.03 0.88 0.75 0.55 ---- | 0.94 1.04 0.99 0.91 ---- |
| 0.6        | 1.10 1.25 1.41 1.43 ---- | 1.11 1.26 1.41 1.39 ---- | 0.71 0.60 0.59 0.46 ---- | 1.03 0.91 0.80 0.55 ---- | 0.94 1.04 1.00 0.90 ---- |
| 0.7        | 1.11 1.28 1.44 1.49 ---- | 1.12 1.28 1.45 1.42 ---- | 0.73 0.58 0.54 0.46 ---- | 0.93 0.95 0.75 0.56 ---- | 0.93 1.06 1.02 0.87 ---- |
| 0.8        | 1.11 1.30 1.49 1.51 ---- | 1.13 1.29 1.48 1.46 ---- | 0.69 0.67 0.48 0.36 ---- | 0.87 0.97 0.70 0.63 ---- | 0.86 1.17 1.03 0.98 ---- |
| 0.9        | 1.13 1.31 1.52 1.53 ---- | 1.14 1.30 1.54 1.44 ---- | 0.62 0.74 0.49 0.43 ---- | 0.70 0.94 0.66 0.55 ---- | 0.78 1.35 1.06 0.98 ---- |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_64.txt-->
<!--vs-x86/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | for_each                 | visit_all                | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.96 0.96 0.95 0.90 ---- | 0.95 1.06 1.25 1.14 ---- | 0.63 0.61 0.59 0.54 ---- | 0.94 0.95 0.89 0.63 ---- | 1.02 1.06 1.02 0.95 ---- |
| 0.1        | 0.98 1.08 1.26 1.01 ---- | 0.98 1.09 1.27 1.21 ---- | 0.64 0.62 0.59 0.54 ---- | 0.91 0.88 0.82 0.61 ---- | 0.95 1.05 1.00 0.93 ---- |
| 0.2        | 1.01 1.11 1.30 1.09 ---- | 1.01 1.11 1.34 1.24 ---- | 0.64 0.61 0.58 0.53 ---- | 0.91 0.86 0.79 0.59 ---- | 0.95 1.04 1.00 0.87 ---- |
| 0.3        | 1.04 1.15 1.34 1.19 ---- | 1.03 1.15 1.38 1.29 ---- | 0.64 0.61 0.57 0.51 ---- | 0.92 0.84 0.76 0.57 ---- | 0.94 1.05 1.00 0.91 ---- |
| 0.4        | 1.07 1.19 1.38 1.29 ---- | 1.06 1.19 1.44 1.34 ---- | 0.63 0.60 0.56 0.50 ---- | 0.95 0.86 0.72 0.55 ---- | 0.94 1.05 1.01 0.87 ---- |
| 0.5        | 1.09 1.24 1.43 1.36 ---- | 1.08 1.24 1.48 1.38 ---- | 0.65 0.64 0.59 0.50 ---- | 0.98 0.86 0.74 0.54 ---- | 0.95 1.05 1.01 0.92 ---- |
| 0.6        | 1.11 1.28 1.47 1.42 ---- | 1.09 1.27 1.47 1.42 ---- | 0.68 0.63 0.57 0.46 ---- | 0.96 0.87 0.77 0.54 ---- | 0.94 1.06 1.01 0.89 ---- |
| 0.7        | 1.12 1.30 1.50 1.45 ---- | 1.10 1.29 1.46 1.48 ---- | 0.68 0.58 0.51 0.50 ---- | 0.89 0.89 0.69 0.59 ---- | 0.93 1.07 1.04 0.84 ---- |
| 0.8        | 1.13 1.32 1.63 1.53 ---- | 1.12 1.30 1.49 1.46 ---- | 0.68 0.64 0.47 0.58 ---- | 0.87 0.95 0.66 0.62 ---- | 0.86 1.23 1.05 1.01 ---- |
| 0.9        | 1.15 1.34 1.62 1.53 ---- | 1.13 1.32 1.51 1.49 ---- | 0.59 0.71 0.48 0.43 ---- | 0.71 0.92 0.61 0.54 ---- | 0.79 1.40 1.11 1.09 ---- |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_80.txt-->

## Reference

### `<boost/container/hub.hpp>`

Defines [`boost::container::hub`](#class-template-boostcontainerhub) and associated functions.

```cpp
namespace boost {
namespace container {

using from_range_t = /* implementation-defined */;
inline constexpr from_range_t from_range {};

template<typename T, typename Allocator = std::allocator<T>>
  class hub;

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

* The library requires C++11 at a minimum. `std::uint64_t` must exist.
* `from_range_t` and `from_range` are only available if the standard library provides
  `<ranges>` and `<concepts>`. If this is the case, `boost::container::from_range_t`
  is equal to  C++23 [`std::from_range_t`](https://en.cppreference.com/w/cpp/ranges/from_range.html)
  if this is provided; otherwise, it is a different type with the same characteristics.
* `boost::container::pmr::hub` is only available if the standard library provides
  `<memory_resource>`.


### Class template `boost::container::hub`

`boost::container::hub` — A container with constant-time insertion and erasure and
element stability. `boost::container::hub<T, Allocator>` is a model of
[`SequenceContainer`](https://en.cppreference.com/w/cpp/named_req/SequenceContainer.html),
[`ReversibleContainer`](https://en.cppreference.com/w/cpp/named_req/ReversibleContainer.html) and
[`AllocatorAwareContainer`](https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer.html),
with the following exceptions:

* Operators `==` and `!=` are not provided.
* Positional insertion operations are not provided: `emplace(position, args...)`, `insert(position, first, last)`, etc. 

The iterators of `hub` are models of
[`LegacyBidirectionalIterator`](https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator).

Elements of a `hub` are stored in _blocks_ of contiguous memory with a capacity of 64 elements each.
Insertion position is determined by the container, and insertion may reuse the memory locations
of previously erased elements. A block with at least one element is called _active_; a block
without any element is called _reserved_. When an active block becomes empty after element
erasure, the container keeps it internally as a reserved block for future reuse rather than deallocating it.
Reserved blocks may only be deallocated with `shrink_to_fit` or `trim_capacity` (or on container destruction).
Reserved blocks are not used until all active blocks are full. New blocks are only allocated when 
all the blocks in the container are full or if the user issues a `reserve` operation.

#### Synopsis

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
  hub(const hub& x, const std::type_identity_t<Allocator>& alloc);
  hub(hub&&) noexcept;
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
    -> hub<typename std::iterator_traits<InputIterator>::value_type, Allocator>;

template<
  std::ranges::input_range R,
  typename Allocator = std::allocator<std::ranges::range_value_t<R>>
>
  hub(from_range_t, R&&, Allocator = Allocator())
    -> hub<std::ranges::range_value_t<R>, Allocator>;

} // namespace container
} // namespace boost
```

* User-defined deduction guides are ony available if the compiler supports CTAD. 
* range-related operations are only available if the standard library provides
  `<ranges>` and `<concepts>`.

#### Description
##### Template parameters

| Parameter | Description |
|--|--|
| `T` | The cv-unqualified object type of the elements inserted into the container. |
| `Allocator` | An [`Allocator`](https://en.cppreference.com/w/cpp/named_req/Allocator) whose value type is `T`. |

##### Exception Safety Guarantees

Except when explicitly noted, all non-const member functions and associated functions taking
`boost::container::hub` by non-const reference provide the
[basic exception guarantee](https://en.cppreference.com/w/cpp/language/exceptions#Exception_safety),
whereas all const member functions and associated functions taking
`boost::container::hub` by const reference provide the
[strong exception guarantee](https://en.cppreference.com/w/cpp/language/exceptions#Exception_safety).

Except when explicitly noted, no operation throws an exception unless that exception
is thrown by the container’s `Allocator` object (if any).

#### Constructors, copy and assignment

`hub() noexcept(noexcept(Allocator()));`

_Preconditions:_ `Allocator` must be [`DefaultConstructible`](https://en.cppreference.com/w/cpp/named_req/DefaultConstructible).<br/>
_Effects:_ Constructs an empty `hub`, using `Allocator()` as the allocator.<br/>
_Complexity:_ Constant.

`explicit hub(const Allocator&) noexcept;`

_Effects:_ Constructs an empty `hub`, using the specified allocator.<br/>
_Complexity:_ Constant.

`explicit hub(size_type n, const Allocator& = Allocator());`

_Preconditions:_ `Tp` is [`DefaultInsertable`](https://en.cppreference.com/w/cpp/named_req/DefaultInsertable.html) into `hub`. <br/>
_Effects:_ Constructs a `hub` with `n` default-inserted elements, using the specified allocator. <br/> 
_Complexity:_ Linear in `n`.

`hub(size_type n, const T& value, const Allocator& = Allocator());`

_Preconditions:_ `T` is [`CopyInsertable`](https://en.cppreference.com/w/cpp/named_req/CopyInsertable.html) into `hub`. <br/>
_Effects:_ Constructs a `hub` with `n` copies of `value`, using the specified allocator. <br/>
_Complexity:_ Linear in `n`.

`template<typename InputIterator>`<br/>
`  hub(InputIterator first, InputIterator last, const Allocator& = Allocator());`

_Effects:_ Constructs a `hub` equal to the range `[first, last)`, using the specified allocator. <br/>
_Complexity:_ Linear in `std::distance(first, last)`.

`template</* container-compatible-range<T> */ R>`<br/>
`  hub(from_range_t, R&& rg, const Allocator& = Allocator());`

_Effects:_ Constructs a `hub` object equal to the range `rg`, using the specified allocator. <br/>
_Complexity:_ Linear in `std::ranges​::​distance(rg)`.

`hub(const hub& x);`<br/>
`hub(const hub& x, const std::type_identity_t<Allocator>& alloc);`

_Preconditions:_ `T` is [`CopyInsertable`](https://en.cppreference.com/w/cpp/named_req/CopyInsertable.html) into `hub`. <br/>
_Effects:_ Constructs a `hub` object equal to `x`. If the second overload is called, uses `alloc`.
_Complexity:_ Linear in `x.size()`.

`hub(hub&&) noexcept;`<br/>
`hub(hub&&, const std::type_identity_t<Allocator>& alloc);`

_Preconditions:_ For the second overload, when `std::allocator_traits<Allocator>​::​is_always_equal​::​value` is `false`, `T` meets the [`MoveInsertable`](https://en.cppreference.com/w/cpp/named_req/MoveInsertable) requirements. <br/>
_Effects:_ When the first overload is called, or the second overload is called and `alloc == x.get_allocator()` is true, element block is moved from `x` into `*this`.
Pointers and references to the elements of `x` now refer to those same elements but as members of `*this`.
Iterators referring to the elements of `x` will continue to refer to their elements, but they now behave as iterators into `*this`. <br/>
If the second overload is called and `alloc == x.get_allocator()` is `false`, each element in `x` is moved into `*this`.
References, pointers and iterators referring to the elements of `x` are invalidated. <br/>
_Postconditions:_ `x.empty()` is `true`.
The relative order of the elements of `*this` is the same as that of the elements of `x` prior to the call. <br/>
_Complexity:_ If the second overload is called and `alloc == x.get_allocator()` is `false`, linear in `x.size()`.
Otherwise constant.

`hub(std::initializer_list<T> il, const Allocator& = Allocator());`

_Preconditions:_ `T` is [`CopyInsertable`](https://en.cppreference.com/w/cpp/named_req/CopyInsertable.html) into `hub`. <br/>
_Effects:_ Constructs a `hub` object equal to `il`, using the specified allocator. <br/>
_Complexity:_ Linear in `il.size()`.

`hub& operator=(const hub& x);`

_Preconditions:_ `T`  is [`CopyInsertable`](https://en.cppreference.com/w/cpp/named_req/CopyInsertable.html) into `hub`
and [`CopyAssignable`](https://en.cppreference.com/w/cpp/named_req/CopyAssignable). <br/>
_Effects:_ All elements in `*this` are either copy-assigned to, or destroyed.
All elements in `x` are copied into `*this`, maintaining their relative order. <br/>
_Complexity:_ Linear in `size() + x.size()`.

`hub& operator=(hub&& x)`<br/>
`  noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||`<br/>
`           std::allocator_traits<Allocator>::is_always_equal::value);`

_Preconditions:_ When `(allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
 allocator_traits<Allocator>::is_always_equal::value)`
is `false`, `T` is [`MoveInsertable`](https://en.cppreference.com/w/cpp/named_req/MoveInsertable) into `hub`
and [`MoveAssignable`](https://en.cppreference.com/w/cpp/named_req/MoveAssignable). <br/>
_Effects:_ Each element in `*this` is either move-assigned to, or destroyed.
When `(allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
 get_allocator() == x.get_allocator())`
is `true`, each element block is moved from `x` into `*this`.
Pointers and references to the elements of `x` now refer to those same elements but as members of `*this`.
Iterators referring to the elements of `x` will continue to refer to their elements, but they now behave as iterators into `*this`, not into `x`. <br/>
When `(allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
 get_allocator() == x.get_allocator())`
is `false`, each element in `x` is moved into `*this`.
References, pointers and iterators referring to the elements of `x` are invalidated. <br/>
_Postconditions:_ `x.empty()` is `true`.
The relative order of the elements of `*this` is the same as that of the elements of `x` prior to this call. <br/>
_Complexity:_ Linear in `size()`.
If `(allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
 get_allocator() == x.get_allocator())`
is `false`, also linear in `x.size()`.

<a name="ref-capacity"></a>
#### Capacity

`size_type capacity() const noexcept;`

_Returns:_ The total number of elements that `*this` can hold without requiring allocation of more element blocks. <br/>
_Complexity:_ Constant.

`void reserve(size_type n);`

_Effects:_ If `n <= capacity()` is `true`, there are no effects.
Otherwise increases `capacity()` by allocating reserved blocks. <br/>
_Postconditions:_ `capacity() >= n` is `true`. <br/>
_Throws:_ `std::length_error` if `n > max_size()`, as well as any exceptions thrown by the allocator. <br/>
_Complexity:_ Linear in the number of reserved blocks allocated. <br/>
_Remarks:_ All references, pointers, and iterators referring to elements in `*this`, as well as the past-the-end iterator, remain valid.

`void shrink_to_fit();`

_Preconditions:_ `T` is [`MoveInsertable`](https://en.cppreference.com/w/cpp/named_req/MoveInsertable) into `hub`. <br/>
_Effects:_ Reallocates elements if needed so that the number of active blocks is minimized and deallocates all ensuing reserved blocks.
If `capacity()` is already equal to `size()`, there are no effects. If an exception is thrown by `T` during reallocation, the effects are unspecified. <br/>
_Complexity:_ If reallocation happens, linear in the size of the sequence. Also, linear in the number of reserved blocks. <br/>
_Remarks:_ If reallocation happens, the order of the elements in `*this` may change and all references, pointers, and iterators referring
to the elements in `*this` are invalidated.

`void trim_capacity() noexcept;`<br/>
`void trim_capacity(size_type n) noexcept;`

_Effects:_ For the first overload, all reserved blocks are deallocated, and `capacity()` is reduced accordingly.
For the second overload, if `n >= capacity()` is `true`, there are no effects; otherwise, `capacity()` is reduced to no less than `n`. <br/>
_Complexity:_ Linear in the number of non-full blocks. <br/>
_Remarks:_ All references, pointers, and iterators referring to elements in `*this`, as well as the past-the-end iterator, remain valid.

#### Modifiers

`template<typename... Args>`<br/>
`  iterator emplace(Args&&... args);`<br/>
`template<typename... Args>`<br/>
`  iterator emplace_hint(const_iterator hint, Args&&... args);`

_Preconditions:_ T is [`EmplaceConstructible`](https://en.cppreference.com/w/cpp/named_req/EmplaceConstructible) into `hub` from `args`. <br/>
_Effects:_ Inserts an object of type `T` constructed with `std​::​forward<Args>(args)...`.
The `hint` parameter is ignored.
If an exception is thrown, there are no effects.<br/>
(Note: `args` can directly or indirectly refer to a value in `*this`.) <br/>
_Returns:_ An iterator that points to the new element. <br/>
_Complexity:_ Constant. Exactly one object of type `T` is constructed. <br/>

`iterator insert(const T& x);`<br/>
`iterator insert(T&& x);`<br/>
`iterator insert(const_iterator hint, const T& x);`<br/>
`iterator insert(const_iterator hint, T&& x);`<br/>

_Effects:_ Equivalent to: `return emplace(std​::​forward<decltype(x)>(x));`

`void insert(std::initializer_list<T> il);`

_Effects:_ Equivalent to: `insert(il.begin(), il.end());`

`template</* container-compatible-range<T> */ R>`<br/>
`  void insert_range(R&& rg);`

_Preconditions:_ `T` is [`EmplaceConstructible`](https://en.cppreference.com/w/cpp/named_req/EmplaceConstructible) into `hub` from `*ranges​::​begin(rg)`.
`rg` and `*this` do not overlap. <br/>
_Effects:_ Inserts copies of elements in `rg`.
Each iterator in the range `rg` is dereferenced exactly once. <br/>
_Complexity:_ Linear in the number of elements inserted.
Exactly one object of type `T` is constructed for each element inserted.

`template<typename InputIterator>`<br/>
`  void insert(InputIterator first, InputIterator last);`

_Preconditions:_ `T` is [`EmplaceConstructible`](https://en.cppreference.com/w/cpp/named_req/EmplaceConstructible) into `hub` from `*first`.
[`first`, `last`) and `*this` do not overlap. <br/>
_Effects:_ Inserts copies of elements in [`first`, `last`).
Each iterator in the range [`first`, `last`) is dereferenced exactly once. <br/>
_Complexity:_ Linear in the number of elements inserted.
Exactly one object of type `T` is constructed for each element inserted.

`void insert(size_type n, const T& x);`

_Preconditions:_ `T` is [`CopyInsertable`](https://en.cppreference.com/w/cpp/named_req/CopyInsertable.html) into `hub`. <br/>
_Effects:_ Inserts `n` copies of `x`. <br/>
_Complexity:_ Linear in `n`.
Exactly one object of type `T` is constructed for each element inserted.

`iterator erase(const_iterator position);`<br/>
`iterator erase(const_iterator first, const_iterator last);`

_Complexity:_ Linear in the number of elements erased. <br/>
_Remarks:_ Invalidates references, pointers and iterators referring to the erased elements.

`void erase_void(const_iterator position);`

_Effects:_ Equivalent to: `erase(position);` <br/>
(Note: Potentially faster than `erase(position)` since no return iterator needs to be computed.)

`void swap(hub&)`<br/>
`  noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value ||`<br/>
`           std::allocator_traits<Allocator>::is_always_equal::value);`

_Effects:_ Exchanges the contents and `capacity()` of `*this` with those of `x`. <br/>
_Complexity:_ Constant.

<a name="ref-stdhive-operations"></a>
#### `std::hive` operations

`void splice(hub& x);`<br/>
`void splice(hub&& x);`

_Preconditions:_ `get_allocator() == x.get_allocator()` is `true`. <br/>
_Effects:_ If `std::addressof(x) == this` is `true`, the behavior is erroneous and there are no effects.
Otherwise, inserts the contents of `x` into `*this` and `x` becomes empty.
Pointers and references to the moved elements of `x` now refer to those same elements but as members of `*this`.
Iterators referring to the moved elements continue to refer to their elements, but they now behave as iterators into `*this`, not into `x`. <br/>
_Complexity:_ Linear in the sum of all element blocks in `x` plus all element blocks in `*this`. <br/>
_Remarks:_ Reserved blocks in `x` are not transferred into `*this`.

`template<typename BinaryPredicate = std::equal_to<T>>`<br/>
`  size_type unique(BinaryPredicate pred = BinaryPredicate());`

_Preconditions:_ `pred` is an equivalence relation. <br/>
_Effects:_ Erases all but the first element from every consecutive group of equivalent elements.
That is, for a nonempty `hub`, erases all elements referred to by the iterator `i` in the range [`begin() + 1`, `end()`) 
for which `pred(*i, *(i - 1))` is `true`. <br/>
_Returns:_ The number of elements erased. <br/>
_Throws:_ Nothing unless an exception is thrown by the predicate. <br/>
_Complexity:_ If `empty()` is `false`, exactly `size() - 1` applications of the corresponding predicate, otherwise no applications of the predicate. <br/>
_Remarks:_ Invalidates references, pointers, and iterators referring to the erased elements.

`template<typename Compare = std::less<T>>`<br/>
`  void sort(Compare comp = Compare());`

_Preconditions:_ `T` is is [`MoveInsertable`](https://en.cppreference.com/w/cpp/named_req/MoveInsertable) into `hub`,
[`MoveAssignable`](https://en.cppreference.com/w/cpp/named_req/MoveAssignable), 
and [`Swappable`](https://en.cppreference.com/w/cpp/named_req/Swappable). <br/>
_Effects:_ Sorts `*this` according to the `comp` function object.
If an exception is thrown, the order of the elements in `*this` is unspecified. <br/>
_Complexity:_ O(<i>N</i>·log<i>N</i>) comparisons, where _N_ is `size()`. <br/>
_Remarks:_ May allocate.
References, pointers, and iterators referring to elements in `*this` may be invalidated. <br/>
(Note: The sorting algorithm used is not stable.)

`iterator get_iterator(const_pointer p) noexcept;`<br/>
`const_iterator get_iterator(const_pointer p) const noexcept;`

_Preconditions:_ `p` points to an element in `*this`. <br/>
_Returns:_ An `iterator` or `const_iterator` pointing to the same element as `p`. <br/>
_Complexity:_ Linear in the number of active blocks in _*this_.

<a name="ref-internal-visitation"></a>
####   Internal visitation

`template<typename F>`<br/>
`  void visit(iterator first, iterator last, F f);`<br/>
`template<typename F>`<br/>
`  void visit(const_iterator first, const_iterator last, F f) const;`

_Preconditions:_ [`first`, `last`) is a valid range on `*this`. <br/>
_Effects:_ Equivalent to: `while(first != last) f(*first++);` <br/>
(Note: Potentially faster than the sample code due to internal optimizations.)

`template<typename F>`<br/>
`  iterator visit_while(iterator first, iterator last, F f);`<br/>
`template<typename F>`<br/>
`  const_iterator visit_while(const_iterator first, const_iterator last, F f) const;`

_Preconditions:_ [`first`, `last`) is a valid range on `*this`. <br/>
_Effects:_ Equivalent to:
```cpp
 while(first != last) {
   if(!f(*first)) return first;
   else ++first;
 }
 return last;
```
(Note: Potentially faster than the sample code due to internal optimizations.)

`template<typename F>`<br/>
`  void visit_all(F f);`<br/>
`template<typename F>`<br/>
`  void visit_all(F f) const;`

_Effects:_ Equivalent to: `visit(begin(), end(), std::ref(f));`

`template<typename F>`<br/>
`  iterator visit_all_while(F f);`<br/>
`template<typename F>`<br/>
`  const_iterator visit_all_while(F f) const;`

_Effects:_ Equivalent to: `return visit_while(begin(), end(), std::ref(f));`

#### Erasure

`template<typename T, typename Allocator, typename U = T>`<br/>
`  typename hub<T, Allocator>::size_type`<br/>
`    erase(hub<T, Allocator>& x, const U& value);`

_Effects:_ Equivalent to: `return erase_if(c, [&](const auto& elem) -> bool { return elem == value; });`

`template<typename T, typename Allocator, typename Predicate>`<br/>
`  typename hub<T, Allocator>::size_type`<br/>
`    erase_if(hub<T, Allocator>& x, Predicate pred);`

_Effects:_ Equivalent to:
```cpp
auto original_size = c.size();
for (auto i = c.begin(); i != c.end(); ) {
  if (pred(*i)) {
    i = c.erase(i);
  } else {
    ++i;
  }
}
return original_size - c.size();
```
(Note: Potentially faster than the sample code due to internal optimizations.)

