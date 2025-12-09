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
project(boost_mp11)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(boost_mp11 INTERFACE)
add_library(Boost::mp11 ALIAS boost_mp11)

set_target_properties(boost_mp11 PROPERTIES EXPORT_NAME mp11)

target_include_directories(boost_mp11 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(boost_mp11 INTERFACE cxx_std_11)

install(DIRECTORY include/boost
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS boost_mp11
    EXPORT boost_mp11-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT boost_mp11-targets
    FILE boost_mp11-targets.cmake
    NAMESPACE Boost::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/boost_mp11
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/boost_mp11-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/boost_mp11-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/boost_mp11
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/boost_mp11-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/boost_mp11
)