# Hub container

[![Branch](https://img.shields.io/badge/branch-develop-brightgreen.svg)](https://github.com/joaquintides/hub/tree/develop) [![CI](https://github.com/joaquintides/hub/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/joaquintides/hub/actions/workflows/ci.yml) [![CI](https://ci.appveyor.com/api/projects/status/github/joaquintides/hub?branch=develop&svg=true)](https://ci.appveyor.com/project/joaquintides/hub) [![Coverage](https://joaquintides.github.io/hub/develop/gcovr/badges/coverage-lines.svg)](https://joaquintides.github.io/hub/develop/gcovr/index.html) <br/>
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
  * [Visitation](#visitation)
  * [Debugging](#debugging)
    * [Visual Studio Natvis](#visual-studio-natvis)
    * [GDB Pretty-Printer](#gdb-pretty-printer)
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
    * [Erasure](#erasure)
    * [Visitation](#ref-visitation)

## Introduction

`boost::container::hub` is a container with constant-time insertion and erasure and _element stability_: 
pointers/iterators to an element remain valid until the element is erased.

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

The observant reader may retort that `std::list` is also stable and provides constant-time insertion/erasure:
the key difference is that `boost::container::hub` is orders of magnitude faster because memory is allocated
in chunks of contiguous elements, which amortizes allocation costs and provides some degree of
cache locality. An important tradeoff when using `boost::container::hub` is the fact that the user can't
control the position where a new element will be inserted: `boost::container::hub` reuses the memory
addresses of previously erased elements to maximize performance and keep the data structure as compact
as possible.

`boost::container::hub` is very similar but not entirely equivalent to C++26
[`std::hive`](https://eel.is/c++draft/sequences#hive) (hence the different naming).
Consult the section ["Deviations from `std::hive`"](#deviations-from-stdhive) for details.

The primary use case for `boost::container::hub`, `std::hive` and similar containers such
as _slot maps_ is in high-performance scenarios where elements are created and destroyed frequently,
insertion order is not relevant and pointer/iterator stability is required: game entity systems,
particle simulation and high-frequency trading come to mind.

## Getting started

`boost::container::hub` depends on Boost. Consult the website [section](https://www.boost.org/doc/user-guide/getting-started.html) on how
to install the entire Boost project or only the exact dependencies of `boost::container::hub`
(`assert`, `config`, `core` and `throw_exception`).

This is a header-only library, so no additional build phase is needed. C++11 or later required.
The library has been verified to work with GCC 4.8, Clang 3.5 and Visual Studio 2017/MSVC 14.1 
(and later versions of those). You can check that your environment is correctly set up by
compiling the example program shown above.

## Tutorial

If you're familiar with STL containers such as `std::list` and `std::vector`,
getting used to `boost::container::hub` is entirely straightforward as its API is
mostly analogous. The key characteristics that set this container apart are:

* Pointers and iterators to an element remain valid as long as the element is not
erased. `hub` will _not_ reallocate elements as it grows in size.
* Insertion and erasure are constant-time and very fast. Memory is allocated in
element blocks with fixed capacity (64 elements per block in this implementation),
and the container keeps track of available positions, including those of erased elements,
to use them for further insertions and keep the number of memory allocations to the
minimum possible.

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
h.reserve(1000); // capacity() is rounded to the next multiple of 64 (1024)
for(int i = 0; i < 500; ++i) h.insert(i); // won't allocate as capacity() >= 500
```

In the example, `h` ends up with 8 non-empty blocks and 8 empty (also called _reserved_)
blocks:

<p align="center"><img src="doc/img/hub_500_1024.png" alt="hub size=500, capacity=1024" height=150 /></p>

Empty blocks can be deallocated as follows:

```cpp
h.trim_capacity(750); // capacity() rounded up to next multiple of 64 no less than 750
```

<p align="center"><img src="doc/img/hub_500_768.png" alt="hub size=500, capacity=768" height=150 /></p>

or with:

```cpp
h.trim_capacity(); // equivalent to trim_capacity(0)
```

<p align="center"><img src="doc/img/hub_500_512.png" alt="hub size=500, capacity=512" height=150 /></p>

Obviously, in this example `h.trim_capacity()` doesn't bring the capacity down to zero
because `h` is not empty.

After erasures, a `hub` may contain "holes" or available positions in
non-empty blocks that can't be trimmed further:

```cpp
erase_if(h, [](int x) { return x % 2 != 0; }); // erase odd values
```

<p align="center"><img src="doc/img/hub_250_512.png" alt="hub size=250, capacity=512" height=150 /></p>

`shrink_to_fit` reallocates elements so that they occupy the minimum possible
number of blocks, and then deallocates the remaining blocks:

```cpp
h.shrink_to_fit();
```

<p align="center"><img src="doc/img/hub_250_256.png" alt="hub size=250, capacity=256" height=150 /></p>

If we print the elements of `h`:

```cpp
for(const auto& x: h) std::cout << x << " ";
```
we get:
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
h1.splice(h2); // transfer non-empty blocks from h2 to h1 (no reallocation)
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
h.erase(it); // erase the element (couldn't be done directly with p)
```

`get_iterator` returns an iterator after a pointer to a valid element of the
`hub`. This can be useful in legacy scenarios where elements of the container
are externally tracked via pointers, or for encapsulation purposes, or
to save memory (`hub` iterators typically are 16 bytes in size). Note, however,
that `get_iterator` is not cheap: execution is linear on the number of
non-empty blocks.

### Visitation

The following, typical processing loop:

```cpp
boost::container::hub<int> h;
//...
for(auto& x: h) x *= 2;
```

can also be written as:

```cpp
// Note this is _not_ std::for_each
for_each(h, [](auto& x) { x *= 2; });
```

Although functionally equivalent to the classical loop, `for_each` is generally
faster as it is implemented with a combination of loop unrolling and prefetching
techniques. Speedups can be as high as 1.75x. Consult the [performance](#performance)
section for a comparison of execution speeds. Consult the
[reference](#ref-visitation) for documentation on variations of
`for_each` (`for_each(first, last, f)`, `for_each_while(h, f)`,
`for_each_while(first, last, f`).

### Debugging
#### Visual Studio Natvis

Add the [`boost_hub.natvis`](extra/boost_hub.natvis) visualizer to your project to allow
for user-friendly inspection of `boost::container::hub`s.

<p align="center"><img src="doc/img/natvis.png" alt="Natvis window" /></p>

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

## Motivation for a novel data structure

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

<p align="center"><img src="doc/img/data_structure.png" alt="diagram" /></p>

* Active blocks are kept in an intrusive doubly-linked list. Block size
is fixed to 64 elements.
* Each block points to its associated element array and maintains a bitmask
of used slots. The reason why a block size of 64 has been chosen is because
the resulting associated bitmask is a 64-bit word, for which most CPU
architectures provide fast bit manipulation instructions.
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

* Minimum and maximum block size limits can't be specified and are fixed
(to 64 in this implementation). `reshape` is not provided as it doesn't make sense when
block capacity is fixed.
* `trim_capacity` is linear on the number of _available_ blocks
(`std::hive::trim_capacity` is linear on the number of _reserved_ blocks,
i.e. those without any used slot).
* Iterators are not
[`three_way_comparable`](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable.html):
Making them so would require extra block metadata and bookkeeping, and this overhead
was not deemed worth imposing over the potential usefulness of having ordered
iterators.
* `get_iterator` is not `noexcept`.
* No operations are marked `constexpr`.

The following functionality is specific to `boost::container::hub`:

* As cache locality is relatively poorer than that of other implementations of `std::hive`
(like `plf::hive`), which can use much larger blocks, iteration performance may suffer.
To partially alleviate this, _visitation_ functions `for_each`,  and
`for_each_while` are provided: these are more performant than regular external
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
* range-based `for` loop traversal of the container after insertion of _n_ elements and
random erasure with probability _r_.
* Visitation-based `for_each` traversal for `boost::container::hub` vs. range `for`
traversal for `plf::hive`.
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
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.27 2.43 1.22 1.38 1.47 | 1.28 1.18 1.19 1.27 1.49 | 1.04 1.03 1.03 1.03 1.04 | 1.79 1.96 1.98 1.97 1.81 | 1.01 1.00 1.00 1.00 1.00 |
| 0.1        | 1.24 1.22 1.26 1.90 1.59 | 1.25 1.18 1.23 1.55 1.56 | 1.04 1.04 1.04 1.03 1.02 | 1.70 1.76 1.76 1.75 1.63 | 1.00 0.98 1.00 0.99 0.97 |
| 0.2        | 1.21 1.25 1.34 2.00 1.72 | 1.21 1.20 1.30 1.81 1.72 | 1.06 1.02 1.04 1.02 1.02 | 1.65 1.73 1.74 1.72 1.54 | 1.00 0.98 1.00 0.98 0.97 |
| 0.3        | 1.23 1.30 1.44 1.78 1.95 | 1.26 1.26 1.41 1.99 1.90 | 1.03 1.01 1.01 1.00 1.00 | 1.74 1.72 1.68 1.68 1.43 | 1.00 0.99 1.00 0.98 0.97 |
| 0.4        | 1.32 1.37 1.54 1.81 2.06 | 1.34 1.30 1.52 1.74 2.02 | 1.00 1.00 1.00 0.99 0.97 | 1.86 1.76 1.64 1.64 1.29 | 1.00 0.98 0.99 0.98 0.96 |
| 0.5        | 1.38 1.52 1.64 1.86 2.17 | 1.40 1.39 1.61 1.81 2.21 | 1.13 0.99 0.97 0.97 0.96 | 1.80 1.82 1.60 1.60 1.17 | 0.99 0.99 0.99 0.96 0.95 |
| 0.6        | 1.46 1.91 1.76 1.89 2.42 | 1.48 1.84 1.72 1.92 2.34 | 1.14 0.99 0.96 0.96 0.95 | 1.80 1.92 1.63 1.57 1.08 | 0.98 1.01 0.99 0.96 0.94 |
| 0.7        | 1.47 1.92 1.88 2.12 2.48 | 1.52 1.83 1.83 2.04 2.45 | 1.31 1.12 0.94 0.93 0.96 | 1.84 2.02 1.67 1.50 1.01 | 0.98 1.32 0.97 0.94 0.91 |
| 0.8        | 1.69 2.09 2.02 2.12 2.56 | 1.55 1.88 1.92 2.02 2.53 | 1.42 1.53 1.03 0.91 0.98 | 1.84 2.05 1.46 1.32 0.92 | 0.94 0.99 0.95 0.91 0.86 |
| 0.9        | 1.55 2.01 2.16 2.17 2.62 | 1.58 1.76 1.95 2.26 2.59 | 1.35 1.63 1.08 0.76 0.88 | 1.63 1.96 1.33 1.08 0.95 | 0.87 0.84 0.91 0.83 0.80 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_16.txt-->
<!--gcc-x64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.02 2.38 2.48 2.20 1.59 | 1.05 0.95 0.97 2.34 1.62 | 1.07 1.05 1.03 0.98 0.97 | 1.87 1.92 1.95 1.30 1.15 | 1.05 1.00 1.01 1.52 1.95 |
| 0.1        | 1.04 1.02 1.09 2.05 1.68 | 1.03 0.97 1.03 2.17 1.68 | 1.09 1.05 1.03 1.05 0.97 | 1.75 1.76 1.74 1.32 1.11 | 1.03 0.99 1.00 1.33 1.87 |
| 0.2        | 1.04 1.03 1.15 2.18 1.87 | 1.02 0.98 1.10 2.24 1.89 | 1.11 1.04 1.03 0.97 0.95 | 1.78 1.77 1.69 1.24 1.13 | 1.03 0.99 0.99 1.27 1.80 |
| 0.3        | 1.05 1.08 1.23 2.19 1.94 | 1.02 1.04 1.18 2.29 2.05 | 1.07 1.04 1.03 1.01 0.95 | 1.81 1.75 1.69 1.19 1.09 | 1.03 0.99 0.99 1.21 1.74 |
| 0.4        | 1.11 1.11 1.30 2.26 2.03 | 1.09 1.08 1.26 2.50 2.16 | 1.04 1.03 1.02 0.96 0.93 | 1.89 1.79 1.67 1.09 1.01 | 1.06 0.99 0.99 1.14 1.67 |
| 0.5        | 1.16 1.19 1.40 2.31 2.11 | 1.12 1.20 1.35 2.49 2.29 | 1.23 1.03 1.02 0.91 0.91 | 1.97 1.92 1.65 1.09 0.97 | 1.01 1.00 0.99 1.05 1.59 |
| 0.6        | 1.22 1.45 1.50 2.43 2.38 | 1.23 1.50 1.46 2.70 2.35 | 1.28 1.06 1.00 0.95 0.92 | 1.99 1.89 1.47 1.08 0.92 | 1.02 1.01 0.99 1.03 1.54 |
| 0.7        | 1.36 1.62 1.62 2.44 2.29 | 1.34 1.58 1.58 2.65 2.45 | 1.48 1.18 0.99 0.91 0.91 | 1.97 1.84 1.28 1.09 0.89 | 0.97 1.56 0.98 0.89 1.48 |
| 0.8        | 1.28 1.71 1.78 2.60 2.49 | 1.25 1.65 1.70 2.74 2.54 | 1.44 1.50 0.96 0.88 0.91 | 1.87 1.90 1.36 1.39 1.01 | 0.92 1.03 0.96 0.83 1.35 |
| 0.9        | 1.55 1.68 1.88 2.64 2.51 | 1.52 1.55 1.81 2.80 2.58 | 1.37 1.63 1.08 0.75 0.86 | 1.66 2.08 1.39 1.19 1.01 | 0.78 0.78 0.90 0.83 1.08 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_32.txt-->
<!--gcc-x64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.04 3.20 2.65 2.99 1.94 | 1.04 0.97 1.00 1.04 1.93 | 1.08 1.05 1.04 0.92 0.95 | 1.85 1.70 1.84 1.01 0.98 | 1.01 1.00 1.01 1.54 1.83 |
| 0.1        | 1.04 1.05 1.11 2.69 1.92 | 1.04 0.99 1.06 1.52 1.94 | 1.12 1.08 1.08 0.90 0.93 | 1.88 1.72 1.74 1.00 0.94 | 1.01 0.99 1.00 1.48 1.73 |
| 0.2        | 1.03 1.08 1.20 2.75 2.11 | 1.03 1.01 1.13 1.69 2.10 | 1.17 1.09 1.07 0.89 0.90 | 1.84 1.55 1.50 0.97 0.91 | 1.02 1.00 1.01 1.32 1.65 |
| 0.3        | 1.06 1.14 1.26 2.81 2.21 | 1.04 1.08 1.21 1.78 2.26 | 1.16 1.10 1.09 0.91 0.91 | 1.82 1.49 1.38 1.03 0.89 | 1.02 0.99 1.00 1.24 1.60 |
| 0.4        | 1.13 1.26 1.33 2.71 2.29 | 1.14 1.13 1.29 1.74 2.38 | 1.15 1.06 1.02 0.89 0.91 | 1.81 1.44 1.26 1.03 0.89 | 1.01 0.98 1.01 1.17 1.56 |
| 0.5        | 1.14 1.26 1.40 2.58 2.40 | 1.14 1.22 1.37 1.77 2.41 | 1.31 1.03 0.96 0.99 0.92 | 1.78 1.45 1.20 0.91 0.93 | 1.01 0.99 1.00 1.05 1.48 |
| 0.6        | 1.19 1.53 1.49 2.71 2.49 | 1.20 1.47 1.46 1.94 2.50 | 1.34 1.03 0.98 0.92 0.90 | 1.88 1.60 1.27 0.95 1.00 | 1.01 1.04 0.99 1.05 1.44 |
| 0.7        | 1.24 1.54 1.57 2.74 2.56 | 1.24 1.52 1.56 1.90 2.54 | 1.45 1.12 0.93 0.88 0.90 | 2.01 1.80 1.34 1.11 0.99 | 0.98 1.55 0.98 0.86 1.34 |
| 0.8        | 1.28 1.63 1.68 2.15 2.58 | 1.44 1.57 1.65 2.17 2.69 | 1.44 1.52 0.94 0.93 0.88 | 1.96 2.12 1.44 1.38 1.03 | 0.97 1.02 0.95 0.76 1.18 |
| 0.9        | 1.35 1.62 1.76 2.24 2.68 | 1.46 1.55 1.73 2.21 2.78 | 1.36 1.68 1.12 0.76 0.82 | 1.68 2.15 1.44 1.19 0.96 | 0.75 0.80 0.89 0.87 0.92 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_64.txt-->
<!--gcc-x64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.10 3.56 3.47 3.03 2.21 | 1.06 1.01 1.02 3.29 2.35 | 1.04 1.00 0.99 0.92 0.91 | 1.45 1.40 1.43 0.89 0.97 | 1.00 0.99 1.00 1.36 1.77 |
| 0.1        | 1.05 1.06 1.12 2.69 2.11 | 1.04 1.00 1.06 2.98 2.18 | 1.08 1.04 1.01 0.94 0.91 | 1.43 1.30 1.35 0.93 0.91 | 1.00 0.98 1.00 1.43 1.71 |
| 0.2        | 1.05 1.09 1.19 2.64 2.26 | 1.04 1.02 1.13 3.01 2.33 | 1.10 1.02 0.99 0.92 0.91 | 1.44 1.27 1.31 0.92 0.90 | 1.01 0.98 0.98 1.34 1.61 |
| 0.3        | 1.07 1.14 1.26 2.76 2.33 | 1.05 1.07 1.19 2.90 2.36 | 1.09 0.99 0.96 0.94 0.91 | 1.48 1.21 1.18 0.93 0.89 | 1.01 0.98 0.99 1.11 1.58 |
| 0.4        | 1.13 1.20 1.32 2.73 2.45 | 1.10 1.16 1.27 2.79 2.43 | 1.02 0.97 0.94 0.89 0.93 | 1.49 1.08 1.03 0.92 0.89 | 0.99 0.97 0.98 1.17 1.52 |
| 0.5        | 1.16 1.42 1.41 2.70 2.53 | 1.15 1.35 1.35 3.02 2.57 | 1.19 0.99 0.94 0.94 0.97 | 1.54 1.14 1.04 0.92 0.96 | 0.99 0.97 0.98 1.11 1.42 |
| 0.6        | 1.24 1.52 1.50 2.74 2.61 | 1.20 1.45 1.46 3.06 2.66 | 1.24 1.01 0.97 0.92 0.97 | 1.61 1.48 1.15 0.94 0.96 | 0.98 0.99 0.98 0.91 1.30 |
| 0.7        | 1.32 1.53 1.61 3.03 2.67 | 1.28 1.48 1.54 2.92 2.69 | 1.37 1.12 0.89 0.88 0.94 | 1.56 1.63 1.13 0.99 0.95 | 0.93 1.54 0.97 0.80 1.23 |
| 0.8        | 1.36 1.62 1.70 3.00 2.71 | 1.33 1.52 1.63 3.07 2.50 | 1.39 1.37 0.91 0.92 0.90 | 1.60 1.73 1.15 1.42 0.96 | 0.92 1.01 0.95 0.75 1.14 |
| 0.9        | 1.44 1.59 1.77 2.95 2.61 | 1.38 1.52 1.70 3.26 2.74 | 1.29 1.59 1.07 0.73 0.90 | 1.43 1.94 1.14 0.88 0.92 | 0.79 0.80 0.90 0.88 1.00 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x64/element_80.txt-->

### Clang 20, x64
<!--clang-x64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.56 2.49 1.37 1.82 1.82 | 1.08 1.57 1.46 1.69 1.84 | 1.95 2.07 2.07 2.07 2.06 | 2.19 2.39 2.38 2.39 2.30 | 1.06 1.03 1.01 1.01 1.00 |
| 0.1        | 1.58 1.47 1.40 2.15 1.75 | 1.15 1.58 1.47 1.93 1.75 | 2.01 1.99 1.79 1.78 1.78 | 2.27 2.40 2.26 2.04 1.99 | 1.05 1.01 1.00 1.00 0.96 |
| 0.2        | 1.69 1.55 1.55 2.26 2.11 | 1.86 1.67 1.60 2.24 2.17 | 1.99 2.02 1.79 1.75 1.74 | 2.21 2.43 2.28 2.02 1.93 | 1.04 1.08 1.00 0.99 0.96 |
| 0.3        | 1.95 1.67 1.67 2.49 2.40 | 1.96 1.73 1.71 2.31 2.39 | 1.98 2.11 1.79 1.72 1.68 | 2.32 2.47 2.32 1.99 1.84 | 1.04 1.00 1.00 0.99 0.95 |
| 0.4        | 2.02 1.75 1.82 2.30 2.62 | 2.06 1.81 1.86 2.88 2.55 | 1.96 2.11 1.82 1.69 1.60 | 2.24 2.50 2.45 1.96 1.75 | 1.03 0.98 0.99 0.99 0.94 |
| 0.5        | 1.30 1.81 1.98 2.53 2.84 | 2.20 1.89 2.02 2.76 2.73 | 1.89 2.14 1.90 1.64 1.49 | 2.24 2.54 2.53 1.91 1.65 | 0.98 0.99 0.99 0.99 0.93 |
| 0.6        | 1.36 1.92 2.17 2.57 2.99 | 2.31 1.95 2.19 2.67 2.80 | 1.94 2.15 2.10 1.57 1.49 | 2.27 2.61 2.53 1.83 1.47 | 0.98 0.97 0.98 0.98 0.91 |
| 0.7        | 1.42 1.99 2.33 2.97 3.08 | 2.39 2.03 2.34 2.86 3.01 | 1.91 2.18 2.07 1.48 1.43 | 2.38 2.73 2.25 1.75 1.25 | 1.01 0.96 0.98 0.97 0.90 |
| 0.8        | 1.52 2.07 2.47 2.86 3.11 | 2.52 2.07 2.47 2.98 2.96 | 1.91 2.14 1.74 1.32 1.07 | 2.37 2.62 1.91 1.53 0.82 | 0.96 0.93 0.96 0.92 0.86 |
| 0.9        | 1.67 2.10 2.60 2.89 3.09 | 2.64 2.19 2.60 2.91 3.04 | 1.66 1.81 1.50 1.08 0.85 | 2.03 2.07 2.00 1.56 1.06 | 0.89 0.84 0.90 0.88 0.76 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_16.txt-->
<!--clang-x64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.70 2.56 3.82 3.18 2.19 | 1.73 1.59 1.49 3.29 2.18 | 1.99 2.07 2.06 1.88 1.77 | 2.26 2.39 2.38 2.14 2.09 | 1.10 1.02 1.03 1.62 2.38 |
| 0.1        | 1.67 1.62 1.60 2.92 2.00 | 1.74 1.62 1.54 2.87 2.06 | 1.99 1.94 1.77 1.61 1.39 | 2.32 2.40 2.26 1.89 1.85 | 1.09 0.99 1.01 1.52 2.27 |
| 0.2        | 1.75 1.56 1.78 3.16 2.35 | 1.82 1.70 1.72 3.39 2.44 | 2.02 1.99 1.76 1.58 1.31 | 2.30 2.43 2.26 1.83 1.70 | 1.10 0.95 1.01 1.45 2.17 |
| 0.3        | 1.87 1.84 1.93 3.66 2.73 | 1.90 1.78 1.86 3.78 2.66 | 1.99 2.06 1.77 1.54 1.33 | 2.37 2.46 2.23 1.77 1.63 | 1.09 1.10 1.00 1.36 2.07 |
| 0.4        | 1.94 1.93 2.07 3.60 2.83 | 1.99 1.87 1.98 3.41 2.85 | 2.00 2.09 1.76 1.51 1.42 | 2.26 2.47 2.28 1.68 1.48 | 1.08 1.09 1.01 1.28 1.96 |
| 0.5        | 2.01 1.60 2.14 3.41 3.06 | 2.02 1.95 2.10 3.38 2.98 | 1.93 2.09 1.73 1.54 1.35 | 2.33 2.53 2.08 1.45 1.24 | 1.08 1.01 1.00 1.21 1.91 |
| 0.6        | 2.11 1.61 2.31 3.42 3.02 | 2.10 1.99 2.25 3.36 3.07 | 2.00 2.10 1.70 1.30 1.12 | 2.36 2.55 1.85 1.31 1.00 | 1.08 1.05 1.00 1.13 1.81 |
| 0.7        | 2.16 1.59 2.53 4.00 3.16 | 2.17 2.07 2.42 3.34 3.14 | 2.07 2.05 1.49 0.98 0.98 | 2.50 2.51 1.68 1.55 0.92 | 1.07 1.03 0.99 1.06 1.75 |
| 0.8        | 2.26 2.23 2.67 3.45 3.06 | 2.26 2.13 2.59 3.33 3.14 | 1.86 2.01 1.43 1.34 0.99 | 2.32 2.39 1.64 1.61 1.16 | 1.01 1.01 0.98 1.01 1.57 |
| 0.9        | 2.31 1.99 2.77 3.39 3.20 | 2.28 2.21 2.67 3.30 3.15 | 1.55 1.99 1.63 1.05 0.93 | 2.16 2.30 2.08 1.59 1.15 | 0.82 0.89 0.92 0.87 1.25 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_32.txt-->
<!--clang-x64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 2.12 5.72 5.51 4.35 3.02 | 1.91 1.49 1.69 1.72 2.69 | 1.98 2.03 1.96 1.20 1.02 | 2.22 2.31 2.03 0.97 0.91 | 1.01 1.01 1.01 2.05 2.32 |
| 0.1        | 2.12 2.14 1.98 3.60 2.47 | 1.95 1.53 1.79 2.29 2.39 | 2.01 1.91 1.74 1.06 1.00 | 2.28 2.31 1.89 1.02 1.00 | 1.00 0.99 1.01 1.82 2.15 |
| 0.2        | 2.28 2.13 2.18 3.83 2.96 | 2.01 1.59 1.95 2.63 2.76 | 1.99 1.98 1.69 0.95 1.01 | 2.28 2.42 1.83 1.01 0.96 | 0.99 1.00 1.00 1.49 2.05 |
| 0.3        | 2.35 2.27 2.29 3.97 2.99 | 2.07 1.84 2.06 2.84 2.90 | 1.96 1.87 1.62 0.94 0.96 | 2.23 2.25 1.70 0.80 0.92 | 1.01 1.39 0.99 1.47 1.95 |
| 0.4        | 2.43 2.22 2.43 3.11 2.93 | 2.17 1.92 2.17 2.90 2.91 | 1.91 1.87 1.52 0.93 0.93 | 2.17 2.36 1.56 0.85 0.85 | 1.01 1.00 0.99 1.34 1.85 |
| 0.5        | 2.49 2.25 2.54 3.19 3.09 | 2.25 2.06 2.28 2.72 2.64 | 1.86 2.01 1.38 0.97 0.92 | 2.18 2.33 1.49 1.00 0.93 | 0.99 1.00 0.99 1.24 1.80 |
| 0.6        | 2.56 2.43 2.72 2.97 2.98 | 2.33 2.15 2.41 2.69 2.74 | 1.91 2.01 1.35 0.94 0.98 | 2.30 2.34 1.47 1.15 1.05 | 0.98 1.00 0.99 1.16 1.64 |
| 0.7        | 2.63 2.60 2.94 2.99 2.89 | 2.40 2.22 2.58 2.65 2.66 | 1.93 2.02 1.38 1.00 1.01 | 2.36 2.34 1.50 1.42 1.07 | 0.98 0.99 0.98 1.22 1.63 |
| 0.8        | 2.69 2.11 2.92 2.85 2.92 | 2.48 2.31 2.74 2.62 2.60 | 1.93 2.17 1.52 1.04 1.01 | 2.35 2.46 1.69 1.54 1.07 | 0.94 0.96 0.96 0.93 1.43 |
| 0.9        | 2.78 2.09 3.01 2.75 2.81 | 2.52 2.37 2.78 2.71 2.60 | 1.69 2.00 1.70 1.19 0.89 | 2.00 2.46 2.10 1.59 1.01 | 0.82 0.89 0.91 0.87 1.35 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_64.txt-->
<!--clang-x64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 2.15 6.48 6.08 4.09 3.06 | 2.16 1.32 1.73 4.10 3.09 | 1.71 1.71 1.64 1.05 0.98 | 1.72 1.68 1.56 0.76 0.70 | 1.05 1.00 1.00 1.77 2.18 |
| 0.1        | 2.19 2.10 1.97 3.44 2.42 | 2.14 1.56 1.78 3.28 2.74 | 1.76 1.74 1.53 1.04 0.98 | 1.80 1.74 1.45 0.87 0.90 | 1.03 1.01 1.01 1.58 2.10 |
| 0.2        | 2.22 2.14 2.10 4.28 2.87 | 1.97 1.66 1.92 3.95 2.99 | 1.79 1.75 1.48 0.98 0.96 | 1.81 1.81 1.38 0.82 0.91 | 1.05 1.01 0.99 1.41 1.94 |
| 0.3        | 2.28 2.17 2.25 4.17 3.23 | 2.26 1.66 2.03 4.13 2.90 | 1.79 1.81 1.35 0.92 0.90 | 1.79 1.84 1.30 0.76 0.85 | 1.04 1.29 0.99 1.31 1.84 |
| 0.4        | 2.41 2.39 2.23 4.17 3.24 | 2.32 1.95 2.11 3.87 3.07 | 1.75 1.79 1.20 0.96 0.95 | 1.80 1.75 1.30 0.98 0.97 | 1.05 1.03 1.00 1.26 1.67 |
| 0.5        | 2.51 1.86 2.43 3.96 3.12 | 2.40 1.87 2.24 3.57 3.02 | 1.71 1.85 1.19 1.03 1.04 | 1.79 1.90 1.30 1.18 1.10 | 1.03 1.04 0.99 1.19 1.63 |
| 0.6        | 2.52 2.59 2.62 3.69 3.02 | 2.27 2.03 2.38 3.71 2.96 | 1.78 1.83 1.25 1.00 1.04 | 1.89 1.87 1.34 1.26 1.13 | 1.06 1.03 0.98 1.17 1.57 |
| 0.7        | 2.56 2.64 2.77 3.50 2.93 | 2.50 2.01 2.50 3.56 2.92 | 1.65 1.80 1.35 1.03 1.03 | 1.88 1.99 1.46 1.40 1.10 | 1.05 1.01 0.98 0.99 1.54 |
| 0.8        | 2.67 2.19 2.99 3.45 2.99 | 2.41 2.10 2.65 3.31 2.83 | 1.61 1.89 1.55 1.73 1.03 | 1.89 2.17 1.73 0.87 1.14 | 1.03 0.98 0.96 0.96 1.29 |
| 0.9        | 2.74 2.30 3.12 3.42 2.72 | 2.58 2.16 2.75 3.41 2.69 | 1.48 1.82 1.54 1.08 0.98 | 1.76 2.15 1.95 1.65 1.10 | 0.86 0.91 0.92 0.82 1.06 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x64/element_80.txt-->

### Clang 17, ARM64
<!--clang-arm64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.05 1.25 1.23 1.25 1.40 | 1.39 2.19 1.39 1.33 1.21 | 1.79 1.92 1.88 1.88 1.92 | 2.20 2.39 2.37 2.37 2.40 | 1.01 1.45 1.18 1.01 1.01 |
| 0.1        | 1.32 0.95 1.37 1.36 1.19 | 1.40 1.86 1.42 1.48 1.34 | 1.48 1.48 1.40 1.44 1.43 | 2.19 2.31 2.26 2.31 2.32 | 0.97 1.50 1.04 0.98 0.95 |
| 0.2        | 1.25 1.49 1.43 1.70 1.42 | 1.41 2.08 1.47 1.43 1.54 | 1.52 1.42 1.37 1.35 1.42 | 2.17 2.31 2.24 2.27 2.33 | 0.97 0.85 0.99 0.97 0.93 |
| 0.3        | 1.58 1.57 1.19 1.55 1.87 | 1.42 2.58 1.56 1.56 1.48 | 1.74 1.44 1.39 1.37 1.42 | 2.51 2.35 2.26 2.25 2.31 | 0.98 0.70 1.11 0.97 0.93 |
| 0.4        | 1.07 1.59 1.43 1.82 1.42 | 1.46 2.09 1.86 1.67 1.63 | 1.74 1.44 1.35 1.37 1.39 | 2.50 2.32 2.21 2.26 2.27 | 0.96 0.74 1.09 0.96 0.92 |
| 0.5        | 1.28 1.92 1.67 1.70 1.50 | 1.48 2.32 1.70 1.75 1.67 | 2.16 1.65 1.34 1.35 1.37 | 3.05 2.63 2.21 2.16 2.27 | 0.96 0.77 1.10 0.99 0.91 |
| 0.6        | 1.48 1.95 1.62 1.70 1.63 | 1.51 2.41 1.78 1.74 1.66 | 2.24 1.76 1.33 1.29 1.35 | 3.07 2.94 2.29 2.09 2.20 | 0.96 0.64 1.17 0.98 0.90 |
| 0.7        | 1.43 1.84 1.69 1.76 1.70 | 1.52 1.85 1.86 1.77 1.65 | 2.26 2.75 1.43 1.24 1.29 | 2.96 3.78 2.40 1.94 1.97 | 0.93 0.41 0.95 0.98 0.87 |
| 0.8        | 1.46 1.90 1.72 1.75 1.68 | 1.61 1.99 1.86 1.79 1.69 | 2.32 2.65 1.88 1.17 1.23 | 2.71 3.18 2.64 1.50 1.45 | 0.86 2.21 0.99 0.98 0.81 |
| 0.9        | 1.50 1.95 1.75 1.77 1.56 | 1.60 3.03 1.91 1.77 1.72 | 1.77 3.59 1.61 1.16 1.21 | 1.79 4.59 1.78 1.24 1.22 | 0.74 1.45 1.49 0.95 0.75 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_16.txt-->
<!--clang-arm64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.28 1.14 1.25 1.23 1.18 | 1.43 2.56 1.74 1.32 1.20 | 1.75 1.91 1.83 1.78 1.93 | 2.50 2.79 2.87 2.77 2.79 | 1.10 1.07 1.14 1.99 2.70 |
| 0.1        | 1.31 1.52 1.33 1.63 1.37 | 1.47 2.42 1.65 1.51 1.52 | 1.46 1.51 1.46 1.33 1.38 | 2.06 2.31 2.31 2.32 2.30 | 1.05 1.04 1.08 1.84 2.32 |
| 0.2        | 1.34 1.68 1.46 1.73 1.57 | 1.47 1.78 1.48 1.93 1.70 | 1.51 1.44 1.42 1.36 1.37 | 2.30 2.25 2.29 2.02 1.89 | 1.05 0.91 1.14 2.19 2.69 |
| 0.3        | 1.39 1.86 1.54 1.77 1.64 | 1.46 3.53 1.83 1.91 1.65 | 1.82 1.44 1.39 1.42 1.39 | 2.54 2.42 2.36 1.96 1.96 | 1.05 1.12 0.97 2.14 2.94 |
| 0.4        | 1.44 1.99 1.61 1.77 1.66 | 1.55 3.19 1.86 1.84 1.72 | 1.77 1.46 1.33 1.35 1.39 | 2.73 2.38 2.33 1.85 1.74 | 1.07 0.88 0.98 1.91 2.56 |
| 0.5        | 1.48 2.03 1.67 1.71 1.66 | 1.61 2.39 2.00 1.71 1.63 | 2.12 1.70 1.27 1.30 1.42 | 2.88 2.83 2.29 1.70 1.52 | 1.04 1.01 1.13 1.56 2.35 |
| 0.6        | 1.53 2.12 1.70 1.71 1.67 | 1.47 3.24 1.97 1.72 1.64 | 2.43 1.70 1.35 1.27 1.30 | 3.14 2.99 2.39 1.41 1.40 | 1.02 1.08 1.05 1.40 2.21 |
| 0.7        | 1.55 2.14 1.77 1.69 1.59 | 1.63 3.30 1.91 1.79 1.77 | 1.99 2.26 1.37 1.04 1.33 | 2.94 2.91 2.52 1.15 1.14 | 0.97 0.72 1.21 1.24 2.04 |
| 0.8        | 1.61 2.07 1.83 1.63 1.61 | 1.78 3.44 1.86 1.77 1.71 | 2.44 1.84 1.81 1.25 1.24 | 2.74 2.59 2.27 1.13 1.13 | 0.93 2.56 1.08 1.14 1.82 |
| 0.9        | 1.66 1.98 1.85 1.66 1.47 | 1.69 3.56 1.91 1.80 1.64 | 1.72 3.82 2.23 1.17 1.16 | 1.89 4.62 2.42 1.37 1.30 | 0.77 2.47 0.92 0.88 1.37 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_32.txt-->
<!--clang-arm64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.52 1.37 1.50 1.36 1.15 | 1.51 1.47 1.66 1.33 1.23 | 1.81 1.94 1.83 1.69 1.73 | 2.20 2.41 2.25 1.54 1.77 | 1.02 0.95 1.14 1.92 2.62 |
| 0.1        | 1.57 1.46 1.52 1.61 1.44 | 1.56 1.39 1.75 1.70 1.47 | 1.49 1.42 1.44 1.42 1.41 | 2.17 2.38 2.23 1.50 1.49 | 1.01 0.96 1.10 1.83 2.55 |
| 0.2        | 1.62 1.69 1.49 1.86 1.61 | 1.58 1.61 1.66 1.83 1.57 | 1.52 1.41 1.40 1.34 1.34 | 2.20 2.41 2.18 1.34 1.31 | 1.04 0.94 1.06 1.89 2.47 |
| 0.3        | 1.66 1.93 1.63 1.87 1.66 | 1.61 1.79 1.77 1.75 1.67 | 1.68 1.42 1.34 1.23 1.30 | 2.47 2.45 2.29 1.17 1.18 | 1.03 0.94 1.03 1.64 2.37 |
| 0.4        | 1.72 2.02 1.77 1.84 1.68 | 1.68 1.89 1.83 1.81 1.68 | 1.77 1.46 1.33 1.09 1.13 | 2.54 2.48 2.15 1.08 1.07 | 1.03 0.93 1.05 1.53 2.33 |
| 0.5        | 1.77 2.09 1.81 1.82 1.64 | 1.72 1.91 1.91 1.76 1.74 | 2.17 1.68 1.33 1.04 1.10 | 3.04 2.63 2.18 1.03 0.99 | 1.02 0.91 1.05 1.44 2.15 |
| 0.6        | 1.82 2.07 1.90 1.77 1.64 | 1.76 2.01 1.91 1.70 1.66 | 2.27 1.75 1.35 1.01 1.05 | 3.16 2.61 2.20 1.04 1.01 | 1.01 0.88 1.16 1.28 1.98 |
| 0.7        | 1.84 2.12 2.01 1.73 1.62 | 1.81 1.97 1.88 1.73 1.64 | 2.30 2.18 1.45 1.01 1.11 | 2.97 2.66 2.08 1.01 1.00 | 0.99 0.42 1.10 1.22 1.76 |
| 0.8        | 1.88 2.10 2.03 1.67 1.58 | 1.86 2.02 2.03 1.56 1.60 | 2.35 2.36 1.77 1.04 1.07 | 2.76 2.72 2.32 1.21 1.14 | 0.94 0.72 1.04 1.02 1.55 |
| 0.9        | 1.90 2.07 2.05 1.62 1.59 | 1.87 1.96 2.01 1.51 1.56 | 1.70 3.50 2.24 1.21 1.06 | 1.99 4.66 2.57 1.56 1.17 | 0.80 0.84 1.03 0.86 1.21 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_64.txt-->
<!--clang-arm64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.55 1.36 1.60 1.32 1.20 | 1.47 1.43 1.60 1.28 1.19 | 1.80 1.80 1.59 1.27 1.17 | 2.40 2.75 2.32 1.20 1.37 | 1.00 1.01 1.05 1.90 2.61 |
| 0.1        | 1.60 1.41 1.61 1.56 1.36 | 1.51 1.35 1.72 1.46 1.39 | 1.37 1.34 1.45 1.09 1.03 | 2.06 2.39 1.50 1.11 1.20 | 1.00 1.03 0.95 1.63 3.19 |
| 0.2        | 1.66 1.50 1.73 1.71 1.44 | 1.55 1.50 1.76 1.56 1.45 | 1.44 1.31 1.24 1.02 0.98 | 2.30 2.52 1.84 1.16 1.08 | 0.95 0.92 1.35 1.60 2.49 |
| 0.3        | 1.70 1.56 1.84 1.69 1.49 | 1.60 1.53 1.81 1.66 1.52 | 1.58 1.33 1.36 1.13 0.90 | 2.43 2.43 1.97 1.06 1.00 | 0.99 1.17 0.96 1.65 2.24 |
| 0.4        | 1.79 1.67 1.86 1.77 1.57 | 1.66 1.72 1.89 1.71 1.52 | 1.52 1.37 1.11 0.93 0.83 | 2.58 2.45 1.90 1.02 1.16 | 0.98 0.97 1.19 1.64 2.17 |
| 0.5        | 1.83 1.83 1.95 1.70 1.60 | 1.72 1.70 2.02 1.60 1.36 | 2.02 1.54 1.18 1.13 0.99 | 2.93 2.55 1.86 1.05 1.06 | 1.02 0.97 0.99 1.51 2.03 |
| 0.6        | 1.89 1.87 2.04 1.66 1.57 | 1.70 1.83 2.08 1.50 1.59 | 2.15 1.68 1.17 0.96 0.98 | 3.00 2.67 2.18 1.05 1.14 | 0.99 0.94 0.99 1.35 1.80 |
| 0.7        | 1.92 1.95 2.10 1.55 1.58 | 1.61 2.15 3.12 1.44 1.63 | 2.10 2.00 1.26 0.96 0.94 | 2.85 2.43 2.23 1.14 1.21 | 0.99 0.93 1.05 1.24 1.70 |
| 0.8        | 1.94 2.08 2.19 1.51 1.54 | 1.63 2.32 2.35 1.29 1.53 | 2.31 2.88 1.89 0.99 1.02 | 2.67 3.08 2.61 1.33 1.30 | 0.92 1.03 0.82 0.97 1.48 |
| 0.9        | 1.96 2.15 2.05 1.69 1.56 | 1.62 2.14 1.51 1.74 1.53 | 1.77 3.17 2.29 0.99 0.98 | 1.86 4.40 2.63 1.51 1.18 | 0.79 3.16 0.65 0.86 1.04 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-arm64/element_80.txt-->

### VS 2022, x64
<!--vs-x64/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.84 1.00 1.05 1.04 1.10 | 0.91 0.90 1.00 0.94 1.04 | 1.59 1.63 1.65 1.58 1.19 | 1.16 1.12 1.32 1.23 1.18 | 1.04 1.01 1.01 0.98 0.98 |
| 0.1        | 0.86 0.86 1.00 1.20 1.17 | 0.93 0.92 1.05 1.10 1.12 | 1.41 1.42 1.39 1.36 1.09 | 1.17 1.16 1.20 1.27 1.13 | 1.05 1.00 1.00 0.97 0.96 |
| 0.2        | 0.89 0.93 1.05 1.23 1.21 | 0.96 1.00 1.09 1.19 1.22 | 1.35 1.39 1.41 1.37 1.06 | 1.15 1.17 1.24 1.28 1.13 | 1.07 1.01 1.00 0.95 0.95 |
| 0.3        | 0.91 1.06 1.13 1.32 1.36 | 0.99 1.11 1.17 1.23 1.33 | 1.35 1.41 1.38 1.35 1.01 | 1.17 1.15 1.28 1.30 1.10 | 1.03 1.01 0.98 0.96 0.94 |
| 0.4        | 0.96 1.11 1.19 1.41 1.39 | 1.02 1.20 1.25 1.34 1.35 | 1.35 1.38 1.37 1.30 0.96 | 1.16 1.15 1.33 1.33 1.05 | 1.03 1.00 0.99 0.95 0.93 |
| 0.5        | 1.01 1.23 1.26 1.41 1.46 | 1.08 1.29 1.29 1.45 1.44 | 1.52 1.37 1.33 1.29 0.97 | 1.17 1.18 1.34 1.36 1.04 | 1.03 1.00 0.98 0.95 0.93 |
| 0.6        | 1.04 1.32 1.31 1.49 1.50 | 1.09 1.37 1.35 1.48 1.48 | 1.54 1.36 1.29 1.26 0.97 | 1.13 1.20 1.35 1.34 1.06 | 1.03 0.99 0.98 0.92 0.92 |
| 0.7        | 1.10 1.38 1.37 1.50 1.51 | 1.12 1.44 1.40 1.54 1.55 | 1.84 1.62 1.30 1.18 1.06 | 1.10 1.24 1.31 1.31 1.09 | 0.95 1.01 0.97 0.94 0.91 |
| 0.8        | 1.12 1.42 1.38 1.60 1.58 | 1.16 1.47 1.46 1.64 1.55 | 1.83 2.23 1.40 1.09 1.09 | 1.07 1.31 1.52 1.32 1.16 | 0.90 1.01 0.97 0.90 0.89 |
| 0.9        | 1.15 1.43 1.41 1.58 1.61 | 1.21 1.49 1.47 1.64 1.57 | 1.65 2.88 1.39 1.13 1.14 | 0.92 1.51 1.55 1.28 1.18 | 0.80 1.04 0.95 0.87 0.84 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_16.txt-->
<!--vs-x64/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.87 0.83 1.06 0.98 1.07 | 0.95 0.91 0.99 0.92 1.00 | 1.59 1.62 1.57 1.26 1.01 | 1.14 1.12 1.28 1.18 1.06 | 1.08 1.04 1.01 1.70 2.17 |
| 0.1        | 0.89 0.87 1.02 1.13 1.21 | 0.96 0.92 1.05 1.11 1.15 | 1.36 1.35 1.33 1.07 0.97 | 1.16 1.19 1.34 1.20 1.05 | 1.04 1.03 0.99 1.67 2.08 |
| 0.2        | 0.92 0.90 1.09 1.29 1.28 | 1.00 0.96 1.11 1.25 1.25 | 1.28 1.31 1.25 0.99 0.96 | 1.14 1.21 1.29 1.21 1.03 | 1.01 1.02 1.00 1.67 2.09 |
| 0.3        | 0.97 0.94 1.16 1.35 1.34 | 1.01 1.00 1.17 1.29 1.36 | 1.30 1.29 1.16 0.95 0.97 | 1.16 1.20 1.35 1.11 1.04 | 1.04 1.04 1.01 1.60 2.07 |
| 0.4        | 0.99 1.03 1.23 1.44 1.42 | 1.06 1.08 1.23 1.41 1.41 | 1.26 1.28 1.25 1.10 1.00 | 1.16 1.21 1.34 1.16 1.05 | 1.02 1.03 1.00 1.53 2.02 |
| 0.5        | 1.04 1.13 1.29 1.48 1.46 | 1.09 1.17 1.28 1.53 1.47 | 1.48 1.28 1.17 1.20 1.00 | 1.16 1.23 1.28 1.16 1.07 | 1.04 1.03 0.99 1.48 1.97 |
| 0.6        | 1.09 1.24 1.34 1.58 1.52 | 1.18 1.27 1.35 1.49 1.49 | 1.49 1.25 1.12 1.20 1.05 | 1.12 1.25 1.17 1.32 1.13 | 1.03 1.03 0.97 1.48 1.93 |
| 0.7        | 1.10 1.31 1.41 1.56 1.55 | 1.18 1.36 1.41 1.59 1.54 | 1.84 1.53 1.25 1.13 1.09 | 1.12 1.28 1.27 1.38 1.16 | 1.02 1.03 0.99 1.38 1.88 |
| 0.8        | 1.18 1.37 1.44 1.65 1.55 | 1.25 1.40 1.42 1.59 1.55 | 1.71 2.11 1.27 1.12 1.08 | 1.04 1.33 1.60 1.35 1.19 | 0.95 1.09 0.99 1.28 1.74 |
| 0.9        | 1.22 1.39 1.40 1.68 1.60 | 1.28 1.42 1.42 1.67 1.55 | 1.51 2.57 1.58 1.18 1.04 | 0.94 1.53 1.55 1.31 1.13 | 0.91 1.57 0.97 0.90 1.49 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_32.txt-->
<!--vs-x64/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.91 0.85 1.13 0.96 0.90 | 0.96 0.96 1.03 1.01 0.97 | 1.61 1.63 1.19 0.90 0.93 | 1.22 1.24 1.17 0.92 0.96 | 1.01 1.05 1.03 1.59 1.99 |
| 0.1        | 0.92 0.91 1.07 1.23 1.10 | 1.00 0.98 1.10 1.14 1.12 | 1.43 1.44 1.14 0.92 0.97 | 1.23 1.25 1.03 0.96 0.96 | 0.99 1.04 1.03 1.59 1.93 |
| 0.2        | 0.94 0.97 1.18 1.39 1.29 | 1.00 1.03 1.13 1.21 1.24 | 1.40 1.42 1.10 1.00 0.98 | 1.22 1.32 1.11 1.00 0.97 | 1.01 1.03 1.04 1.51 1.90 |
| 0.3        | 0.98 1.02 1.22 1.53 1.38 | 1.04 1.07 1.25 1.46 1.29 | 1.41 1.42 0.92 0.99 0.98 | 1.23 1.27 1.19 0.99 1.03 | 1.00 1.04 1.01 1.45 1.87 |
| 0.4        | 1.02 1.08 1.32 1.67 1.47 | 1.07 1.13 1.32 1.58 1.38 | 1.39 1.42 0.96 1.00 0.99 | 1.25 1.26 1.09 1.04 1.05 | 1.02 1.06 1.01 1.45 1.78 |
| 0.5        | 1.04 1.17 1.38 1.83 1.53 | 1.08 1.21 1.40 1.65 0.93 | 1.56 1.40 1.02 1.01 0.99 | 1.15 1.27 1.17 1.11 1.06 | 0.99 1.05 1.01 1.44 1.77 |
| 0.6        | 1.08 1.26 1.46 1.91 1.70 | 0.88 1.51 1.18 1.82 1.50 | 1.59 1.38 1.06 1.10 1.00 | 1.14 1.27 1.14 1.08 1.17 | 1.00 1.07 1.05 1.32 1.69 |
| 0.7        | 1.13 1.34 1.56 2.00 1.73 | 1.15 1.35 1.48 1.85 1.62 | 1.84 1.65 1.13 0.93 1.08 | 1.11 1.31 1.22 1.42 1.17 | 1.00 1.08 1.04 1.24 1.68 |
| 0.8        | 1.14 1.36 1.48 1.87 1.72 | 1.16 1.37 1.49 1.76 1.59 | 1.81 2.21 1.27 1.11 1.04 | 1.03 1.35 1.32 1.44 1.13 | 0.99 1.10 1.02 1.17 1.53 |
| 0.9        | 1.16 1.37 1.46 1.76 1.65 | 1.19 1.41 1.50 1.69 1.52 | 1.73 2.84 1.39 1.09 1.07 | 0.94 1.52 1.66 1.23 1.18 | 0.93 1.31 1.04 0.96 1.34 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_64.txt-->
<!--vs-x64/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.98 0.88 1.19 1.03 0.86 | 0.98 1.10 1.22 1.02 0.90 | 1.61 1.62 0.98 1.02 1.00 | 1.30 1.27 1.16 0.91 0.99 | 1.06 1.07 1.04 1.54 1.86 |
| 0.1        | 0.99 0.99 1.15 1.25 0.99 | 1.03 1.13 1.18 1.17 0.95 | 1.42 1.42 1.15 0.91 0.97 | 1.31 1.28 1.10 0.97 0.98 | 1.06 1.06 1.04 1.46 1.80 |
| 0.2        | 1.01 1.07 1.19 1.40 1.13 | 1.04 1.21 1.31 1.30 1.21 | 1.41 1.40 1.28 0.91 0.94 | 1.29 1.29 1.08 1.01 1.00 | 1.06 1.06 1.03 1.39 1.80 |
| 0.3        | 1.05 1.17 1.29 1.52 1.28 | 1.06 1.25 1.32 1.38 1.12 | 1.39 1.38 1.10 0.92 0.96 | 1.30 1.29 1.13 0.99 1.05 | 1.05 1.09 1.01 1.37 1.78 |
| 0.4        | 1.08 1.22 1.35 1.68 1.30 | 1.12 1.30 1.40 1.49 1.20 | 1.37 1.37 1.15 0.93 0.96 | 1.26 1.29 1.04 1.09 1.05 | 1.04 1.10 1.02 1.35 1.69 |
| 0.5        | 1.12 1.28 1.48 1.80 1.40 | 1.13 1.37 1.51 1.60 1.43 | 1.50 1.36 1.20 0.96 1.00 | 1.25 1.30 1.30 1.06 1.12 | 1.03 1.10 1.02 1.29 1.64 |
| 0.6        | 1.12 1.38 1.67 1.87 1.53 | 1.14 1.42 1.68 1.74 1.42 | 1.55 1.35 1.20 1.00 1.01 | 1.23 1.31 1.00 1.09 1.08 | 1.05 1.12 1.02 1.30 1.56 |
| 0.7        | 1.16 1.39 1.55 1.90 1.63 | 1.17 1.45 1.63 1.76 1.45 | 1.83 1.58 1.02 0.93 0.99 | 1.23 1.34 1.24 1.22 1.08 | 1.06 1.18 1.03 1.23 1.56 |
| 0.8        | 1.18 1.42 1.72 1.82 1.55 | 1.20 1.46 1.51 1.72 1.43 | 1.81 2.12 1.12 1.08 1.09 | 1.04 1.37 1.35 1.26 1.13 | 1.06 1.30 1.12 1.14 1.35 |
| 0.9        | 1.19 1.41 1.54 1.70 1.47 | 1.22 1.45 1.50 1.57 1.35 | 1.67 2.77 1.39 1.11 1.06 | 1.00 1.57 1.76 1.26 1.18 | 0.98 2.14 1.24 0.98 1.17 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x64/element_80.txt-->

### GCC 15, x86
<!--gcc-x86/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.07 1.27 1.36 1.28 1.32 | 1.05 1.01 1.01 1.03 1.28 | 0.73 0.75 0.71 0.74 0.72 | 1.36 1.52 1.51 1.52 1.49 | 0.97 1.00 1.00 1.02 2.00 |
| 0.1        | 1.07 1.04 1.12 1.35 1.41 | 1.06 1.02 1.03 1.11 1.44 | 0.74 0.70 0.65 0.66 0.65 | 1.36 1.50 1.45 1.31 1.30 | 0.95 0.99 0.99 1.51 1.86 |
| 0.2        | 1.07 1.05 1.14 1.57 1.51 | 1.06 1.02 1.07 1.20 1.54 | 0.74 0.71 0.64 0.64 0.62 | 1.36 1.50 1.47 1.26 1.24 | 0.96 0.99 0.99 1.00 1.83 |
| 0.3        | 1.07 1.06 1.19 1.47 1.65 | 1.05 1.02 1.11 1.27 1.65 | 0.72 0.71 0.70 0.62 0.62 | 1.33 1.50 1.48 1.22 1.18 | 0.95 1.07 0.99 0.94 1.77 |
| 0.4        | 1.08 1.07 1.22 1.50 1.62 | 1.06 1.03 1.14 1.30 1.68 | 0.70 0.72 0.71 0.61 0.62 | 1.33 1.61 1.49 1.18 1.14 | 0.94 1.13 0.99 1.06 1.77 |
| 0.5        | 1.08 1.08 1.25 1.53 1.82 | 1.06 1.04 1.16 1.35 1.74 | 0.69 0.74 0.73 0.59 0.61 | 1.28 1.58 1.48 1.14 1.09 | 0.94 0.95 0.98 0.92 1.66 |
| 0.6        | 1.09 1.04 1.22 1.34 1.95 | 1.07 1.09 1.19 1.39 1.78 | 0.70 0.76 0.74 0.56 0.63 | 1.27 1.55 1.50 1.08 1.03 | 0.92 0.91 0.97 0.86 1.53 |
| 0.7        | 1.10 1.05 1.26 1.36 1.95 | 1.08 1.06 1.22 1.36 1.88 | 0.70 0.79 0.76 0.55 0.67 | 1.26 1.52 1.47 1.00 1.00 | 0.87 0.94 0.97 0.81 1.36 |
| 0.8        | 1.11 1.09 1.29 1.39 1.97 | 1.09 1.05 1.25 1.45 1.74 | 0.71 0.83 0.79 0.51 0.76 | 1.24 1.57 1.38 0.87 0.87 | 0.81 0.90 0.94 0.91 1.24 |
| 0.9        | 1.12 1.10 1.30 1.40 1.75 | 1.10 1.07 1.26 1.41 1.73 | 0.74 0.89 0.77 0.47 0.57 | 1.13 1.54 1.19 0.85 0.73 | 0.72 0.75 0.88 0.82 0.97 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_16.txt-->
<!--gcc-x86/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.06 1.44 1.51 1.41 1.38 | 1.04 1.01 1.02 1.04 1.50 | 0.72 0.69 0.70 0.72 0.73 | 1.44 1.56 1.52 1.23 0.97 | 0.95 0.99 0.99 1.03 1.59 |
| 0.1        | 1.06 1.04 1.14 1.56 1.44 | 1.04 1.01 1.03 1.11 1.47 | 0.74 0.71 0.65 0.64 0.66 | 1.48 1.51 1.48 1.14 0.90 | 0.94 0.98 0.99 0.99 1.55 |
| 0.2        | 1.06 1.04 1.16 1.55 1.51 | 1.04 1.01 1.05 1.40 1.56 | 0.73 0.71 0.66 0.64 0.66 | 1.40 1.50 1.46 1.05 0.87 | 0.94 0.98 0.99 0.99 1.50 |
| 0.3        | 1.06 1.05 1.20 1.75 1.65 | 1.05 1.02 1.09 1.43 1.72 | 0.74 0.75 0.68 0.66 0.68 | 1.30 1.47 1.48 1.13 0.93 | 0.93 0.98 0.99 0.94 1.45 |
| 0.4        | 1.08 1.06 1.25 1.77 1.69 | 1.04 1.02 1.13 1.41 1.68 | 0.75 0.72 0.70 0.67 0.72 | 1.40 1.49 1.49 1.13 1.11 | 0.93 1.24 0.99 1.00 1.36 |
| 0.5        | 1.09 1.08 1.20 1.64 1.86 | 1.05 1.03 1.16 1.45 1.91 | 0.74 0.75 0.73 0.67 0.77 | 1.30 1.51 1.46 1.15 1.11 | 0.92 0.99 0.98 0.86 1.28 |
| 0.6        | 1.10 1.08 1.23 1.60 1.89 | 1.06 1.04 1.19 1.73 1.78 | 0.76 0.77 0.76 0.68 0.82 | 1.34 1.54 1.40 0.93 0.97 | 0.91 0.95 0.97 0.81 1.23 |
| 0.7        | 1.11 1.10 1.27 1.62 2.05 | 1.07 1.05 1.22 1.75 1.72 | 0.74 0.80 0.77 0.73 0.79 | 1.26 1.57 1.21 0.94 0.89 | 0.89 0.92 0.95 0.76 1.09 |
| 0.8        | 1.12 1.11 1.29 1.57 1.85 | 1.09 1.06 1.24 1.69 2.07 | 0.79 0.83 0.76 0.67 0.60 | 1.24 1.55 1.09 0.81 0.77 | 0.86 0.85 0.92 0.91 1.00 |
| 0.9        | 1.13 1.15 1.29 1.75 1.62 | 1.10 1.09 1.24 1.64 1.70 | 0.77 0.88 0.79 0.48 0.50 | 1.15 1.58 1.26 0.88 0.67 | 0.70 0.74 0.85 0.84 0.84 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_32.txt-->
<!--gcc-x86/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.10 1.06 1.80 1.70 1.52 | 1.07 1.03 1.03 1.05 1.56 | 0.77 0.73 0.70 0.75 0.80 | 1.39 1.43 1.47 1.11 0.98 | 0.93 0.96 0.98 0.93 1.28 |
| 0.1        | 1.10 1.08 1.14 1.74 1.58 | 1.07 1.04 1.05 1.12 1.63 | 0.74 0.73 0.68 0.70 0.74 | 1.43 1.52 1.46 1.02 0.96 | 0.92 0.96 0.98 0.85 1.18 |
| 0.2        | 1.09 1.09 1.18 1.80 1.65 | 1.07 1.04 1.07 1.26 1.71 | 0.75 0.74 0.67 0.70 0.81 | 1.44 1.52 1.50 0.95 0.96 | 0.92 0.96 0.98 0.88 1.14 |
| 0.3        | 1.09 1.12 1.21 1.88 1.72 | 1.07 1.05 1.11 1.35 1.76 | 0.76 0.75 0.71 0.72 0.84 | 1.44 1.55 1.37 0.78 0.88 | 0.92 1.01 0.97 0.84 1.11 |
| 0.4        | 1.10 1.09 1.23 2.00 1.84 | 1.08 1.06 1.13 1.52 1.74 | 0.76 0.77 0.75 0.74 0.85 | 1.43 1.57 1.25 0.83 0.82 | 0.91 0.97 0.96 0.87 1.08 |
| 0.5        | 1.11 1.11 1.26 2.02 1.92 | 1.08 1.08 1.16 1.61 1.92 | 0.76 0.76 0.77 0.78 0.77 | 1.39 1.54 1.13 0.85 0.81 | 0.91 1.02 0.94 0.83 1.02 |
| 0.6        | 1.11 1.11 1.32 2.07 1.85 | 1.08 1.06 1.19 1.65 1.70 | 0.78 0.80 0.74 0.58 0.66 | 1.36 1.54 1.07 0.76 0.79 | 0.90 0.89 0.95 0.78 1.00 |
| 0.7        | 1.12 1.14 1.35 2.15 1.91 | 1.09 1.07 1.21 1.70 1.74 | 0.78 0.81 0.73 0.51 0.62 | 1.25 1.55 1.08 0.68 0.76 | 0.87 0.90 0.90 0.71 0.93 |
| 0.8        | 1.13 1.13 1.28 1.75 1.95 | 1.10 1.07 1.22 1.65 1.77 | 0.72 0.82 0.76 0.46 0.55 | 1.25 1.58 1.17 0.90 0.72 | 0.83 0.84 0.87 0.90 0.87 |
| 0.9        | 1.15 1.18 1.29 1.73 1.49 | 1.10 1.12 1.25 1.74 1.47 | 0.75 0.88 0.84 0.49 0.51 | 1.26 1.60 1.30 0.89 0.65 | 0.74 0.74 0.79 0.88 0.70 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_64.txt-->
<!--gcc-x86/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.06 1.05 1.85 1.77 1.66 | 1.05 1.01 1.02 1.09 1.70 | 0.76 0.73 0.73 0.85 0.93 | 1.32 1.40 1.33 0.89 0.92 | 0.97 0.98 1.00 0.85 1.03 |
| 0.1        | 1.05 1.04 1.11 1.75 1.63 | 1.05 1.00 1.02 1.14 1.67 | 0.76 0.75 0.71 0.78 0.88 | 1.34 1.40 1.33 0.95 0.96 | 0.97 0.97 0.98 0.75 0.90 |
| 0.2        | 1.05 1.04 1.13 1.77 1.71 | 1.04 1.01 1.04 1.30 1.71 | 0.76 0.75 0.71 0.85 0.90 | 1.46 1.42 1.28 0.88 0.93 | 0.97 0.97 0.97 0.71 0.87 |
| 0.3        | 1.05 1.05 1.16 1.85 1.75 | 1.03 1.01 1.08 1.42 1.86 | 0.76 0.74 0.72 0.91 0.90 | 1.36 1.42 1.18 0.85 0.85 | 0.98 0.97 0.97 0.71 0.86 |
| 0.4        | 1.06 1.04 1.22 1.92 1.83 | 1.03 1.02 1.11 1.53 1.94 | 0.76 0.76 0.72 0.91 0.84 | 1.34 1.43 1.08 0.90 0.83 | 0.98 0.97 0.98 0.68 0.84 |
| 0.5        | 1.04 1.05 1.21 1.99 1.81 | 1.03 1.02 1.13 1.61 1.98 | 0.79 0.76 0.72 0.81 0.72 | 1.31 1.44 1.04 0.86 0.84 | 0.98 1.02 0.97 0.66 0.82 |
| 0.6        | 1.05 1.07 1.28 2.05 1.81 | 1.04 1.02 1.16 1.61 1.71 | 0.75 0.77 0.70 0.59 0.67 | 1.30 1.44 1.01 0.78 0.82 | 0.98 0.92 0.96 0.63 0.79 |
| 0.7        | 1.05 1.06 1.26 2.08 1.92 | 1.04 1.04 1.18 1.65 1.86 | 0.75 0.76 0.71 0.52 0.63 | 1.29 1.43 1.05 0.69 0.80 | 0.97 0.91 0.95 0.58 0.74 |
| 0.8        | 1.07 1.07 1.22 1.65 2.03 | 1.05 1.05 1.20 1.62 1.77 | 0.74 0.79 0.74 0.60 0.59 | 1.19 1.46 1.09 0.75 0.76 | 0.95 0.87 0.93 0.89 0.66 |
| 0.9        | 1.09 1.09 1.23 1.66 1.54 | 1.08 1.05 1.21 1.67 1.36 | 0.75 0.85 0.81 0.47 0.56 | 1.16 1.50 1.21 0.83 0.71 | 0.87 0.78 0.86 0.80 0.58 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--gcc-x86/element_80.txt-->

### Clang 20, x86
<!--clang-x86/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.08 1.27 1.43 1.37 1.24 | 1.11 1.07 1.08 1.19 1.31 | 0.64 0.63 0.63 0.63 0.65 | 1.10 1.18 1.18 1.17 1.18 | 1.05 1.04 1.02 1.46 2.62 |
| 0.1        | 1.11 1.08 1.19 1.47 1.28 | 1.14 1.10 1.12 1.23 1.31 | 0.64 0.62 0.60 0.60 0.62 | 1.11 1.18 1.14 1.06 1.06 | 1.00 1.02 1.00 1.28 2.45 |
| 0.2        | 1.14 1.11 1.23 1.72 1.40 | 1.17 1.13 1.17 1.40 1.38 | 0.64 0.63 0.60 0.60 0.62 | 1.10 1.18 1.13 1.05 1.04 | 1.00 1.03 1.00 1.32 2.39 |
| 0.3        | 1.15 1.14 1.28 1.62 1.48 | 1.18 1.15 1.22 1.45 1.45 | 0.64 0.64 0.61 0.59 0.62 | 1.09 1.19 1.14 1.03 1.03 | 1.00 1.10 1.00 1.24 2.24 |
| 0.4        | 1.18 1.16 1.32 1.58 1.53 | 1.20 1.19 1.27 1.37 1.57 | 0.64 0.64 0.62 0.59 0.64 | 1.08 1.19 1.18 1.01 1.02 | 0.99 0.89 0.99 1.14 2.14 |
| 0.5        | 1.21 1.18 1.38 1.68 1.64 | 1.23 1.20 1.33 1.40 1.64 | 0.63 0.64 0.62 0.59 0.66 | 1.06 1.20 1.20 1.02 1.02 | 0.99 0.90 0.99 1.19 2.00 |
| 0.6        | 1.24 1.21 1.44 1.65 1.65 | 1.26 1.24 1.39 1.47 1.66 | 0.63 0.65 0.64 0.57 0.72 | 1.06 1.21 1.20 0.95 1.05 | 0.95 0.90 0.98 1.01 1.81 |
| 0.7        | 1.27 1.24 1.42 1.57 1.70 | 1.29 1.27 1.43 1.52 1.69 | 0.64 0.66 0.65 0.56 0.82 | 1.06 1.22 1.21 0.96 1.12 | 0.93 0.90 0.97 0.96 1.68 |
| 0.8        | 1.31 1.26 1.47 1.55 1.72 | 1.31 1.29 1.48 1.55 1.71 | 0.64 0.69 0.68 0.53 0.84 | 1.06 1.25 1.21 1.01 0.98 | 0.87 0.87 0.94 0.90 1.45 |
| 0.9        | 1.32 1.28 1.49 1.48 1.68 | 1.34 1.32 1.50 1.52 1.68 | 0.66 0.76 0.70 0.50 0.67 | 1.02 1.37 1.09 1.03 0.80 | 0.78 0.80 0.88 0.86 1.07 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_16.txt-->
<!--clang-x86/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.13 1.56 1.64 1.56 1.37 | 1.13 1.09 1.09 1.24 1.42 | 0.66 0.63 0.62 0.67 0.69 | 1.13 1.17 1.17 1.12 1.23 | 1.01 0.99 1.00 1.52 2.25 |
| 0.1        | 1.17 1.14 1.23 1.55 1.37 | 1.18 1.14 1.13 1.36 1.34 | 0.66 0.62 0.60 0.65 0.68 | 1.14 1.18 1.13 1.00 1.10 | 0.99 0.97 0.98 1.42 2.13 |
| 0.2        | 1.20 1.18 1.29 1.71 1.38 | 1.21 1.17 1.20 1.49 1.52 | 0.66 0.63 0.60 0.66 0.71 | 1.14 1.18 1.13 0.99 1.10 | 0.98 0.97 0.97 1.38 2.03 |
| 0.3        | 1.22 1.20 1.40 2.03 1.48 | 1.23 1.20 1.26 1.53 1.62 | 0.67 0.63 0.60 0.68 0.76 | 1.15 1.18 1.14 1.00 1.09 | 0.98 0.92 0.97 1.38 1.98 |
| 0.4        | 1.25 1.23 1.45 1.93 1.53 | 1.26 1.23 1.32 1.62 1.68 | 0.65 0.63 0.61 0.73 0.85 | 1.16 1.18 1.17 1.04 1.07 | 0.98 1.02 0.97 1.28 1.84 |
| 0.5        | 1.29 1.26 1.39 1.99 1.54 | 1.29 1.25 1.38 1.70 1.68 | 0.65 0.64 0.61 0.82 0.96 | 1.12 1.18 1.16 1.14 1.09 | 0.97 0.91 0.96 1.22 1.72 |
| 0.6        | 1.32 1.30 1.45 1.97 1.50 | 1.32 1.29 1.43 1.78 1.72 | 0.65 0.65 0.63 0.84 1.00 | 1.14 1.19 1.14 1.08 1.06 | 0.95 0.92 0.95 1.16 1.67 |
| 0.7        | 1.34 1.34 1.51 1.83 1.61 | 1.34 1.33 1.49 1.75 1.69 | 0.65 0.67 0.64 0.90 0.81 | 1.12 1.22 1.09 0.93 0.99 | 0.93 0.92 0.94 1.06 1.54 |
| 0.8        | 1.39 1.38 1.57 1.91 1.59 | 1.37 1.36 1.54 1.83 1.79 | 0.64 0.70 0.64 0.55 0.69 | 1.08 1.26 0.99 0.98 0.87 | 0.89 0.90 0.92 0.86 1.38 |
| 0.9        | 1.39 1.42 1.59 1.96 1.46 | 1.39 1.40 1.57 1.82 1.56 | 0.67 0.75 0.68 0.47 0.58 | 1.06 1.32 1.12 0.98 0.81 | 0.75 0.77 0.85 0.82 0.97 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_32.txt-->
<!--clang-x86/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.26 1.30 2.42 2.15 1.73 | 1.32 1.27 1.26 1.30 1.79 | 0.64 0.61 0.61 0.76 0.90 | 1.12 1.16 1.16 0.98 0.95 | 1.01 1.06 1.05 1.26 1.58 |
| 0.1        | 1.29 1.37 1.42 1.90 1.65 | 1.34 1.31 1.28 1.46 1.67 | 0.65 0.61 0.59 0.74 0.88 | 1.06 1.15 1.11 0.96 0.95 | 1.01 1.05 0.98 1.07 1.42 |
| 0.2        | 1.35 1.42 1.52 1.90 1.65 | 1.39 1.35 1.33 1.53 1.71 | 0.66 0.62 0.59 0.75 0.99 | 1.14 1.16 1.10 0.99 0.97 | 1.01 1.06 1.01 1.00 1.28 |
| 0.3        | 1.35 1.44 1.60 1.98 1.60 | 1.40 1.37 1.40 1.61 1.66 | 0.65 0.63 0.61 0.81 0.94 | 1.13 1.17 1.10 0.94 0.94 | 1.00 1.11 1.05 0.94 1.23 |
| 0.4        | 1.40 1.48 1.66 2.08 1.69 | 1.37 1.41 1.45 1.76 1.70 | 0.66 0.65 0.63 0.91 0.92 | 1.15 1.19 1.10 0.92 0.90 | 1.00 1.02 1.04 0.93 1.17 |
| 0.5        | 1.43 1.53 1.72 2.08 1.66 | 1.42 1.45 1.54 1.78 1.72 | 0.66 0.66 0.62 0.88 0.81 | 1.14 1.21 1.02 0.98 0.90 | 0.99 1.08 1.05 0.89 1.13 |
| 0.6        | 1.47 1.57 1.80 2.06 1.68 | 1.43 1.49 1.60 1.78 1.72 | 0.68 0.67 0.63 0.80 0.74 | 1.15 1.22 0.97 0.93 0.86 | 1.00 1.03 1.06 0.84 1.08 |
| 0.7        | 1.48 1.59 1.87 2.06 1.60 | 1.48 1.50 1.66 1.79 1.69 | 0.69 0.67 0.58 0.61 0.65 | 1.09 1.23 0.96 0.81 0.85 | 0.97 0.98 1.00 0.76 1.00 |
| 0.8        | 1.53 1.63 1.78 1.77 1.57 | 1.52 1.55 1.70 1.77 1.68 | 0.65 0.68 0.57 0.51 0.60 | 1.10 1.27 0.95 0.93 0.82 | 0.93 0.92 0.95 0.92 0.91 |
| 0.9        | 1.58 1.68 1.81 1.74 1.39 | 1.56 1.59 1.72 1.72 1.41 | 0.67 0.78 0.69 0.47 0.58 | 1.05 1.36 1.10 0.99 0.81 | 0.83 0.83 0.95 0.88 0.76 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_64.txt-->
<!--clang-x86/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 1.50 1.32 2.71 2.28 1.86 | 1.40 1.34 1.45 2.26 1.87 | 0.63 0.61 0.61 0.85 0.96 | 1.16 1.18 1.21 0.98 0.93 | 1.00 1.03 0.91 1.28 1.49 |
| 0.1        | 1.52 1.48 1.52 2.01 1.70 | 1.42 1.39 1.54 1.88 1.71 | 0.64 0.61 0.59 0.85 0.98 | 1.19 1.23 1.18 0.97 0.98 | 0.97 0.99 1.00 1.12 1.33 |
| 0.2        | 1.55 1.52 1.62 2.06 1.70 | 1.47 1.42 1.50 1.93 1.67 | 0.64 0.62 0.59 0.93 0.98 | 1.20 1.24 1.17 1.08 0.97 | 0.97 1.00 0.99 0.95 1.22 |
| 0.3        | 1.59 1.57 1.65 2.08 1.72 | 1.50 1.45 1.45 1.63 1.70 | 0.63 0.62 0.59 0.81 0.89 | 1.20 1.19 1.11 1.02 0.89 | 0.96 1.01 0.98 0.92 1.15 |
| 0.4        | 1.65 1.61 1.73 2.05 1.73 | 1.55 1.50 1.53 1.79 1.77 | 0.63 0.61 0.57 0.91 0.76 | 1.20 1.25 1.03 0.92 0.87 | 0.96 1.12 0.98 0.85 1.07 |
| 0.5        | 1.68 1.64 1.88 2.16 1.72 | 1.59 1.54 1.60 1.83 1.69 | 0.63 0.62 0.58 0.79 0.67 | 1.19 1.26 0.98 0.93 0.87 | 0.95 1.16 0.97 0.80 1.03 |
| 0.6        | 1.72 1.68 1.94 2.12 1.69 | 1.62 1.57 1.65 1.84 1.71 | 0.63 0.61 0.57 0.64 0.62 | 1.19 1.26 0.96 0.82 0.85 | 0.96 1.08 0.96 0.73 0.95 |
| 0.7        | 1.74 1.70 2.02 2.08 1.70 | 1.64 1.59 1.71 1.85 1.64 | 0.63 0.60 0.56 0.51 0.56 | 1.16 1.23 0.97 0.79 0.85 | 0.96 0.98 0.95 0.64 0.89 |
| 0.8        | 1.78 1.74 1.97 2.05 1.65 | 1.68 1.64 1.72 1.82 1.66 | 0.61 0.61 0.58 0.65 0.53 | 1.11 1.26 1.01 1.16 0.81 | 0.90 0.97 0.91 0.87 0.82 |
| 0.9        | 1.79 1.75 2.04 1.76 1.40 | 1.70 1.66 1.78 1.75 1.32 | 0.63 0.65 0.65 0.49 0.53 | 1.08 1.31 1.16 1.02 0.79 | 0.84 0.82 0.87 0.74 0.70 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--clang-x86/element_80.txt-->

### VS 2022, x86
<!--vs-x86/element_16.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 16                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.99 1.12 1.03 1.07 1.17 | 1.02 0.99 1.01 1.01 1.17 | 0.63 0.61 0.60 0.60 0.53 | 0.95 0.99 0.98 0.96 0.77 | 1.02 1.06 1.05 1.07 1.83 |
| 0.1        | 1.02 0.99 1.02 1.09 1.00 | 1.04 1.01 1.03 1.03 1.16 | 0.62 0.62 0.60 0.60 0.53 | 0.92 0.90 0.88 0.87 0.70 | 0.98 1.04 1.03 1.05 1.79 |
| 0.2        | 1.03 1.01 1.05 1.13 1.27 | 1.05 1.02 1.06 1.11 1.22 | 0.61 0.61 0.59 0.59 0.51 | 0.95 0.89 0.86 0.84 0.67 | 0.98 1.03 1.03 1.03 1.77 |
| 0.3        | 1.05 1.04 1.10 1.15 1.28 | 1.06 1.05 1.09 1.13 1.24 | 0.61 0.60 0.58 0.58 0.48 | 0.94 0.87 0.84 0.82 0.64 | 0.97 1.02 1.02 1.02 1.72 |
| 0.4        | 1.05 1.09 1.14 1.17 1.22 | 1.07 1.09 1.14 1.05 1.23 | 0.61 0.59 0.57 0.57 0.47 | 0.95 0.92 0.82 0.81 0.61 | 0.97 1.01 1.02 1.01 1.64 |
| 0.5        | 1.07 1.14 1.18 1.21 1.35 | 1.08 1.14 1.17 1.22 1.33 | 0.59 0.58 0.56 0.56 0.45 | 0.93 0.92 0.82 0.79 0.57 | 0.96 1.04 1.02 0.99 1.63 |
| 0.6        | 1.09 1.18 1.21 1.24 1.44 | 1.10 1.18 1.21 1.26 1.40 | 0.66 0.56 0.54 0.54 0.44 | 0.94 0.97 0.82 0.76 0.55 | 0.94 1.00 1.01 0.98 1.55 |
| 0.7        | 1.11 1.20 1.24 1.26 1.45 | 1.12 1.20 1.24 1.29 1.42 | 0.66 0.56 0.51 0.51 0.43 | 0.92 0.99 0.82 0.71 0.52 | 0.93 0.99 1.01 0.97 1.51 |
| 0.8        | 1.12 1.21 1.27 1.30 1.48 | 1.14 1.21 1.26 1.29 1.47 | 0.64 0.65 0.49 0.47 0.43 | 0.84 0.99 0.87 0.64 0.51 | 0.89 1.07 0.98 1.00 1.31 |
| 0.9        | 1.14 1.22 1.28 1.29 1.50 | 1.15 1.22 1.27 1.27 1.48 | 0.57 0.71 0.60 0.46 0.47 | 0.69 0.99 0.84 0.58 0.55 | 0.79 1.02 0.93 0.96 1.09 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_16.txt-->
<!--vs-x86/element_32.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 32                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.96 1.02 0.95 0.96 1.18 | 0.97 0.94 0.95 0.94 1.17 | 0.64 0.60 0.59 0.55 0.52 | 1.06 1.07 1.05 0.73 0.73 | 1.05 1.03 1.03 1.09 1.80 |
| 0.1        | 0.95 0.94 0.97 1.00 1.19 | 0.98 0.97 0.98 1.05 1.11 | 0.64 0.61 0.59 0.55 0.52 | 1.01 0.97 0.94 0.75 0.67 | 0.98 1.01 1.01 1.10 1.88 |
| 0.2        | 0.97 0.95 1.01 1.06 1.23 | 1.00 0.98 1.02 1.10 1.27 | 0.63 0.60 0.58 0.54 0.52 | 1.01 0.95 0.92 0.72 0.61 | 0.98 1.01 1.00 1.19 1.89 |
| 0.3        | 0.98 1.00 1.04 1.14 1.33 | 1.01 1.01 1.05 1.11 1.25 | 0.63 0.59 0.58 0.53 0.51 | 1.00 0.93 0.89 0.68 0.64 | 0.98 1.01 1.01 1.16 1.87 |
| 0.4        | 0.99 1.04 1.09 1.19 1.32 | 1.01 1.06 1.09 1.17 1.27 | 0.63 0.58 0.56 0.51 0.51 | 1.05 0.96 0.87 0.68 0.63 | 0.97 1.01 1.04 1.07 1.72 |
| 0.5        | 1.01 1.08 1.13 1.28 1.40 | 1.03 1.10 1.13 1.37 1.37 | 0.66 0.57 0.55 0.50 0.52 | 1.07 0.99 0.85 0.65 0.64 | 0.98 1.01 0.99 1.10 1.74 |
| 0.6        | 1.03 1.13 1.16 1.40 1.47 | 1.05 1.15 1.15 1.47 1.42 | 0.67 0.57 0.53 0.52 0.51 | 1.08 1.06 0.85 0.65 0.61 | 0.97 1.00 0.99 1.07 1.72 |
| 0.7        | 1.05 1.15 1.20 1.36 1.48 | 1.08 1.17 1.20 1.33 1.54 | 0.71 0.58 0.52 0.53 0.53 | 1.04 1.14 0.84 0.73 0.63 | 0.97 1.00 0.99 1.04 1.68 |
| 0.8        | 1.07 1.18 1.23 1.37 1.52 | 1.09 1.19 1.21 1.46 1.47 | 0.68 0.65 0.55 0.53 0.53 | 0.92 1.11 0.90 0.74 0.62 | 0.90 1.11 0.96 0.98 1.54 |
| 0.9        | 1.09 1.19 1.25 1.52 1.47 | 1.11 1.19 1.25 1.37 1.47 | 0.60 0.75 0.56 0.41 0.43 | 0.76 1.03 0.83 0.59 0.56 | 1.43 1.13 0.93 0.95 1.36 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_32.txt-->
<!--vs-x86/element_64.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 64                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.99 1.09 1.00 0.96 ---- | 1.00 1.09 1.24 1.12 ---- | 0.66 0.63 0.61 0.54 ---- | 1.05 1.04 0.99 0.67 ---- | 0.99 1.06 1.00 1.19 ---- |
| 0.1        | 1.01 1.10 1.28 1.05 ---- | 1.02 1.11 1.23 1.11 ---- | 0.68 0.63 0.61 0.52 ---- | 1.01 0.95 0.89 0.61 ---- | 0.94 1.03 1.01 1.04 ---- |
| 0.2        | 1.04 1.13 1.28 1.15 ---- | 1.04 1.13 1.32 1.15 ---- | 0.67 0.62 0.60 0.53 ---- | 1.01 0.93 0.86 0.60 ---- | 0.95 1.03 0.97 0.98 ---- |
| 0.3        | 1.05 1.15 1.37 1.19 ---- | 1.06 1.15 1.30 1.23 ---- | 0.67 0.62 0.58 0.51 ---- | 1.01 0.92 0.84 0.60 ---- | 0.96 1.03 1.04 1.01 ---- |
| 0.4        | 1.07 1.19 1.44 1.27 ---- | 1.08 1.19 1.41 1.31 ---- | 0.67 0.62 0.56 0.49 ---- | 1.06 0.92 0.80 0.59 ---- | 0.95 1.03 1.01 0.92 ---- |
| 0.5        | 1.08 1.22 1.41 1.34 ---- | 1.09 1.23 1.37 1.28 ---- | 0.67 0.60 0.56 0.49 ---- | 1.07 0.93 0.78 0.57 ---- | 0.96 1.03 1.00 0.97 ---- |
| 0.6        | 1.10 1.25 1.41 1.40 ---- | 1.10 1.25 1.37 1.38 ---- | 0.72 0.62 0.53 0.47 ---- | 1.09 0.93 0.83 0.58 ---- | 0.95 1.04 1.02 0.91 ---- |
| 0.7        | 1.11 1.27 1.43 1.43 ---- | 1.10 1.27 1.37 1.39 ---- | 0.72 0.58 0.54 0.45 ---- | 1.02 0.95 0.76 0.53 ---- | 0.94 1.05 1.01 0.93 ---- |
| 0.8        | 1.11 1.29 1.45 1.40 ---- | 1.11 1.28 1.40 1.40 ---- | 0.72 0.66 0.48 0.59 ---- | 0.95 1.07 0.73 0.73 ---- | 0.87 1.20 1.10 1.03 ---- |
| 0.9        | 1.12 1.29 1.49 1.41 ---- | 1.12 1.28 1.39 1.44 ---- | 0.62 0.73 0.48 0.43 ---- | 0.75 1.04 0.66 0.59 ---- | 0.81 1.33 1.16 1.06 ---- |
-----------------------------------------------------------------------------------------------------------------------------------------------------
```
<!--vs-x86/element_64.txt-->
<!--vs-x86/element_80.txt-->
```
             ----------------------------------------------------------------------------------------------------------------------------------------
             | sizeof(element): 80                                                                                                                  |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | insert, erase, insert    | ins, erase, ins, destroy | range for                | for_each                 | sort                     |
             ----------------------------------------------------------------------------------------------------------------------------------------
             | container size           | container size           | container size           | container size           | container size           |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| erase rate | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 | 1.E3 1.E4 1.E5 1.E6 1.E7 |
-----------------------------------------------------------------------------------------------------------------------------------------------------
| 0          | 0.99 1.34 0.96 0.98 ---- | 1.00 1.10 1.31 1.14 ---- | 0.63 0.62 0.59 0.55 ---- | 1.09 1.07 0.99 0.64 ---- | 0.96 1.05 1.01 0.98 ---- |
| 0.1        | 1.01 1.10 1.23 1.00 ---- | 1.02 1.13 1.25 1.16 ---- | 0.64 0.63 0.59 0.54 ---- | 1.04 0.98 0.88 0.63 ---- | 0.91 1.04 1.00 0.96 ---- |
| 0.2        | 1.03 1.12 1.24 1.08 ---- | 1.03 1.15 1.33 1.16 ---- | 0.64 0.61 0.58 0.52 ---- | 1.05 0.94 0.85 0.62 ---- | 0.92 1.03 0.99 0.91 ---- |
| 0.3        | 1.03 1.16 1.32 1.16 ---- | 1.05 1.18 1.33 1.22 ---- | 0.64 0.60 0.56 0.51 ---- | 1.03 0.92 0.80 0.60 ---- | 0.92 1.03 1.00 0.98 ---- |
| 0.4        | 1.06 1.19 1.35 1.24 ---- | 1.07 1.21 1.36 1.28 ---- | 0.64 0.61 0.56 0.50 ---- | 1.10 0.90 0.76 0.58 ---- | 0.92 1.03 1.01 0.97 ---- |
| 0.5        | 1.07 1.24 1.34 1.32 ---- | 1.08 1.25 1.46 1.30 ---- | 0.64 0.63 0.63 0.50 ---- | 1.12 0.92 0.79 0.59 ---- | 0.93 1.04 1.03 0.98 ---- |
| 0.6        | 1.09 1.26 1.40 1.41 ---- | 1.09 1.27 1.43 1.37 ---- | 0.66 0.59 0.60 0.46 ---- | 1.10 0.96 0.83 0.58 ---- | 0.92 1.07 1.03 0.95 ---- |
| 0.7        | 1.10 1.28 1.41 1.40 ---- | 1.10 1.31 1.39 1.37 ---- | 0.68 0.57 0.51 0.45 ---- | 1.03 1.01 0.71 0.58 ---- | 0.91 1.08 1.08 0.91 ---- |
| 0.8        | 1.12 1.31 1.47 1.43 ---- | 1.11 1.31 1.49 1.44 ---- | 0.68 0.65 0.47 0.49 ---- | 0.98 1.08 0.68 0.55 ---- | 0.84 1.23 1.05 1.06 ---- |
| 0.9        | 1.13 1.29 1.43 1.41 ---- | 1.12 1.32 1.46 1.46 ---- | 0.59 0.71 0.48 0.42 ---- | 0.78 1.04 0.63 0.56 ---- | 0.80 1.46 1.18 1.09 ---- |
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

template</* implementation-defined-parameters */, typename F>
  F for_each(/* hub-iterator */ first, /* hub-iterator */ last, F f);
template<typename T, typename Allocator, typename F>
  F for_each(hub<T, Allocator>& x, F f);
template<typename T, typename Allocator, typename F>
  F for_each(const hub<T, Allocator>& x, F f);

template</* implementation-defined-parameters */, typename F>
  std::pair</* hub-iterator */, F>
    for_each_while(/* hub-iterator */ first, /* hub-iterator */ last, F f);
template<typename T, typename Allocator, typename F>
  std::pair<typename hub<T, Allocator>::iterator, F>
    for_each_while(hub<T, Allocator>& x, F f);
template<typename T, typename Allocator, typename F>
  std::pair<typename hub<T, Allocator>::const_iterator, F>
    for_each_while(const hub<T, Allocator>& x, F f);

namespace pmr {

template<typename T>
  using hub = boost::container::hub<T, std::pmr::polymorphic_allocator<T>>;

} // namespace pmr

} // namespace container
} // namespace boost
```
Implementation notes:
* The library requires C++11 at a minimum. `std::uint64_t` must exist.
* `from_range_t` and `from_range` are only available if the standard library provides
  `<ranges>` and `<concepts>`. If this is the case, `boost::container::from_range_t`
  is equal to  C++23 [`std::from_range_t`](https://en.cppreference.com/w/cpp/ranges/from_range.html)
  if this is provided; otherwise, it is a different type with the same characteristics.
* `boost::container::pmr::hub` is only available if the standard library provides
  `<memory_resource>`.

### Class template `boost::container::hub`

`boost::container::hub` — A container with constant-time insertion and erasure and
element stability. `boost::container::hub<T, Allocator>` is a model of
[`Container`](https://en.cppreference.com/w/cpp/named_req/Container.html),
[`ReversibleContainer`](https://en.cppreference.com/w/cpp/named_req/ReversibleContainer.html) and
[`AllocatorAwareContainer`](https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer.html),
with the following exception:

* Operators `==` and `!=` are not provided.

`boost::container::hub<T, Allocator>` is also a model of 
[`SequenceContainer`](https://en.cppreference.com/w/cpp/named_req/SequenceContainer.html),
with the following exception:

* Positional insertion operations of the form `insert(position, ...)` or
`emplace(position, ...)` are not provided or ignore the `position` argument.

The iterators of `hub` are models of
[`LegacyBidirectionalIterator`](https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator).

Elements of a `hub` are stored in _blocks_ of contiguous memory, each with a fixed element capacity.
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

  iterator get_iterator(const_pointer p);
  const_iterator get_iterator(const_pointer p) const;
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
Implementation notes:
* User-defined deduction guides are only available if the compiler supports CTAD. 
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

#### Constructors, copy and assignment

`hub() noexcept(noexcept(Allocator()));`

_Preconditions:_ `Allocator` must be [`DefaultConstructible`](https://en.cppreference.com/w/cpp/named_req/DefaultConstructible).<br/>
_Effects:_ Constructs an empty `hub`, using `Allocator()` as the allocator.<br/>
_Complexity:_ Constant.

`explicit hub(const Allocator&) noexcept;`

_Effects:_ Constructs an empty `hub`, using the specified allocator.<br/>
_Complexity:_ Constant.

`explicit hub(size_type n, const Allocator& = Allocator());`

_Preconditions:_ `T` is [`DefaultInsertable`](https://en.cppreference.com/w/cpp/named_req/DefaultInsertable.html) into `hub`. <br/>
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
_Complexity:_ Linear in `std::ranges::distance(rg)`.

`hub(const hub& x);`<br/>
`hub(const hub& x, const std::type_identity_t<Allocator>& alloc);`

_Preconditions:_ `T` is [`CopyInsertable`](https://en.cppreference.com/w/cpp/named_req/CopyInsertable.html) into `hub`. <br/>
_Effects:_ Constructs a `hub` object equal to `x`. If the second overload is called, uses `alloc`. <br/>
_Complexity:_ Linear in `x.size()`.

`hub(hub&&) noexcept;`<br/>
`hub(hub&&, const std::type_identity_t<Allocator>& alloc);`

_Preconditions:_ For the second overload, when `std::allocator_traits<Allocator>::is_always_equal::value` is `false`, `T` meets the [`MoveInsertable`](https://en.cppreference.com/w/cpp/named_req/MoveInsertable) requirements. <br/>
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
_Complexity:_ If reallocation happens, linear in `size()`. Also, linear in the number of reserved blocks. <br/>
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
_Effects:_ Inserts an object of type `T` constructed with `std::forward<Args>(args)...`.
The `hint` parameter is ignored.
If an exception is thrown, there are no effects.<br/>
(Note: `args` can directly or indirectly refer to a value in `*this`.) <br/>
_Returns:_ An iterator that points to the new element. <br/>
_Complexity:_ Constant. Exactly one object of type `T` is constructed. <br/>

`iterator insert(const T& x);`<br/>
`iterator insert(T&& x);`<br/>
`iterator insert(const_iterator hint, const T& x);`<br/>
`iterator insert(const_iterator hint, T&& x);`<br/>

_Effects:_ Equivalent to: `return emplace(std::forward<decltype(x)>(x));`

`void insert(std::initializer_list<T> il);`

_Effects:_ Equivalent to: `insert(il.begin(), il.end());`

`template</* container-compatible-range<T> */ R>`<br/>
`  void insert_range(R&& rg);`

_Preconditions:_ `T` is [`EmplaceConstructible`](https://en.cppreference.com/w/cpp/named_req/EmplaceConstructible) into `hub` from `*ranges::begin(rg)`.
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
(Note: Potentially faster than `erase(position)` since no return iterator needs to be computed.)

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

_Preconditions:_ `T` is [`MoveInsertable`](https://en.cppreference.com/w/cpp/named_req/MoveInsertable) into `hub`,
[`MoveConstructible`](https://en.cppreference.com/cpp/named_req/MoveConstructible),
[`MoveAssignable`](https://en.cppreference.com/w/cpp/named_req/MoveAssignable), 
and [`Swappable`](https://en.cppreference.com/w/cpp/named_req/Swappable). <br/>
_Effects:_ Sorts `*this` according to the `comp` function object.
If an exception is thrown by `comp` or by any operation on `T`, `*this` is left
in a valid but unspecified state.
If an exception is thrown when the function internally allocates memory, there are no effects. <br/>
_Complexity:_ O(<i>N</i>·log<i>N</i>) comparisons, where _N_ is `size()`. <br/>
_Remarks:_ May allocate.
References, pointers, and iterators referring to elements in `*this` may be invalidated. <br/>
(Note: The sorting algorithm used is not stable.)

`iterator get_iterator(const_pointer p);`<br/>
`const_iterator get_iterator(const_pointer p) const;`

_Preconditions:_ `p` points to an element in `*this`. <br/>
_Returns:_ An `iterator` or `const_iterator` pointing to the same element as `p`. <br/>
_Throws:_ Nothing. <br/>
_Complexity:_ Linear in the number of active blocks in `*this`.

#### Erasure

`template<typename T, typename Allocator, typename U = T>`<br/>
`  typename hub<T, Allocator>::size_type`<br/>
`    erase(hub<T, Allocator>& x, const U& value);`

_Effects:_ Equivalent to: `return erase_if(x, [&](const auto& elem) -> bool { return elem == value; });`

`template<typename T, typename Allocator, typename Predicate>`<br/>
`  typename hub<T, Allocator>::size_type`<br/>
`    erase_if(hub<T, Allocator>& x, Predicate pred);`

_Effects:_ Equivalent to:
```cpp
auto s = x.size();
for(auto i = x.begin(); i != x.end(); ) {
  if(pred(*i)) i = x.erase(i);
  else         ++i;
}
return s - x.size();
```
(Note: Potentially faster than the sample code due to internal optimizations.)

<a name="ref-visitation"></a>
#### Visitation

`template</* implementation-defined-parameters */, typename F>`<br/>
`  F for_each(/* hub-iterator */ first, /* hub-iterator */ last, F f);`

_Constraints:_ `decltype(first)` is the `iterator` or `const_iterator` of an instantiation of `boost::container::hub`.<br/>
_Preconditions:_ [`first`, `last`) is a valid range. <br/>
_Effects:_ Equivalent to:
```cpp
while(first != last) f(*first++);
return f;
```
(Note: Potentially faster than the sample code due to internal optimizations.)

`template<typename T, typename Allocator, typename F>`<br/>
`  F for_each(hub<T, Allocator>& x, F f);`<br/>
`template<typename T, typename Allocator, typename F>`<br/>
`  F for_each(const hub<T, Allocator>& x, F f);`
  
_Effects:_ Equivalent to:
```cpp
boost::container::for_each(x.begin(), x.end(), std::ref(f));
return f;
```

`template</* implementation-defined-parameters */, typename F>`<br/>
`  std::pair</* hub-iterator */, F>`<br/>
`    for_each_while(/* hub-iterator */ first, /* hub-iterator */ last, F f);`

_Constraints:_ `decltype(first)` is the `iterator` or `const_iterator` of an instantiation of `boost::container::hub`.<br/>
_Preconditions:_ [`first`, `last`) is a valid range. <br/>
_Effects:_ Equivalent to:
```cpp
 while(first != last && f(*first)) ++first;
 return {first, std::move(f)};
```
(Note: Potentially faster than the sample code due to internal optimizations.)

`template<typename T, typename Allocator, typename F>`<br/>
`  std::pair<typename hub<T, Allocator>::iterator, F>`<br/>
`    for_each_while(hub<T, Allocator>& x, F f);`<br/>
`template<typename T, typename Allocator, typename F>`<br/>
`  std::pair<typename hub<T, Allocator>::const_iterator, F>`<br/>
`    for_each_while(const hub<T, Allocator>& x, F f);`

_Effects:_ Equivalent to:
```cpp
return {
  boost::container::for_each_while(x.begin(), x.end(), std::ref(f)).first,
  std::move(f)
};
```
