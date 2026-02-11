# Copyright 2021 Alexander Grund
# Copyright 2025-2026 Joaquin M Lopez Munoz
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_subdir_test LANGUAGES CXX)

if(BOOST_CI_INSTALL_TEST)
    find_package(boost_hub REQUIRED)
else()
    add_subdirectory(../.. boostorg/hub)

    set(deps
      # Primary dependencies
      assert
      config
      core
      throw_exception
      # Secondary dependencies
      move
      static_assert
      type_traits
    )

    foreach(dep IN LISTS deps)
      add_subdirectory(../../../${dep} boostorg/${dep})
    endforeach()
endif()

add_executable(main main.cpp)
target_link_libraries(main Boost::hub)

enable_testing()
add_test(NAME main COMMAND main)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)