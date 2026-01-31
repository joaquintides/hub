# Hub container

`boost::container::hub` (proposed for Boost.Container) is a nearly drop-in replacement
of [`std::hive`](https://eel.is/c++draft/sequences#hive) with better performance than
the current reference implementation of this standard component.

## Motivation

`std::hive` was [accepted into C++26](https://herbsutter.com/2025/02/17/trip-report-february-2025-iso-c-standards-meeting-hagenberg-austria)
in February 2025. As of this writing, no major standard library
implementor is providing it yet, though the work to do so is ongoing. Matthew Bentley's
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
(resp. `std::countr_one(mask)`). It is not hard to see that iteration, insertion
and erasure can also be implemented in (non-amortized) constant time.

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
