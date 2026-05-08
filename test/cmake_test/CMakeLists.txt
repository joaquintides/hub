# Copyright 2021 Alexander Grund
# Copyright 2025-2026 Joaquin M Lopez Munoz
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_subdir_test LANGUAGES CXX)

if(BOOST_CI_INSTALL_TEST)
    # Workaround until B2 also installs targets for header-only libs
    if(BOOST_CI_INSTALLED_BY STREQUAL "B2")
        find_package(Boost COMPONENTS headers REQUIRED)
        add_library(boost_hub INTERFACE IMPORTED GLOBAL)
        target_link_libraries(boost_hub INTERFACE Boost::headers)
        add_library(Boost::hub ALIAS boost_hub)
    else()
        find_package(boost_hub REQUIRED)
    endif()
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
