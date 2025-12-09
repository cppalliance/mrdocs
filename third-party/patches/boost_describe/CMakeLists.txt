#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

cmake_minimum_required(VERSION 3.16)
project(boost_describe)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(boost_describe INTERFACE)
add_library(Boost::describe ALIAS boost_describe)

set_target_properties(boost_describe PROPERTIES EXPORT_NAME describe)

target_include_directories(boost_describe INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(boost_describe INTERFACE cxx_std_14)

install(DIRECTORY include/boost
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS boost_describe
    EXPORT boost_describe-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT boost_describe-targets
    FILE boost_describe-targets.cmake
    NAMESPACE Boost::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/boost_describe
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/boost_describe-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/boost_describe-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/boost_describe
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/boost_describe-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/boost_describe
)