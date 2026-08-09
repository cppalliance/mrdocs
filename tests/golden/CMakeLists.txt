#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Golden tests: separate executable; the reference-output harness. Walks
# the fixture corpus under this directory's fixtures/ and runs each test with
# the generator(s) declared in its own configuration. No --generator is
# passed, so each test sets its format.
file(GLOB_RECURSE GOLDEN_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
# fixtures/ holds the corpus consumed by the harness (input .cpp compiled by
# mrdocs at test time), not sources of this executable. Keep them out of the
# harness build.
list(FILTER GOLDEN_TEST_SOURCES EXCLUDE REGEX "/fixtures/")
add_executable(mrdocs-golden-tests ${GOLDEN_TEST_SOURCES})
target_include_directories(mrdocs-golden-tests
        PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>"
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        PRIVATE
        # The harness exercises private library internals. Expose the source
        # (and generated) tree so they include those as <mrdocs/...> rather
        # than reaching in with ../../src/mrdocs/... relative paths. The
        # public include/ above is searched first, so public headers win.
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}/src"
        )
target_link_libraries(mrdocs-golden-tests PUBLIC mrdocs-core test_suite)
# The harness uses MrDocs's private headers, some of which include LLVM.
target_link_libraries(mrdocs-golden-tests PRIVATE mrdocs-llvm)
if (MRDOCS_CLANG)
    target_compile_options(mrdocs-golden-tests PRIVATE -Wno-covered-switch-default)
endif ()
target_compile_definitions(mrdocs-golden-tests PRIVATE -DMRDOCS_TEST_FILES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# The sources #include the generated <mrdocs/ConfigSchema.hpp>
if (TARGET mrdocs-codegen)
    add_dependencies(mrdocs-golden-tests mrdocs-codegen)
endif ()

add_test(NAME mrdocs-golden-tests
    COMMAND
        mrdocs-golden-tests
        --action=test
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
        "--stdlib-includes=${LIBCXX_DIR}"
        "--libc-includes=${CMAKE_SOURCE_DIR}/data/mrdocs/headers/libc-stubs"
        --log-level=warn
)
foreach (action IN ITEMS test create update)
    add_custom_target(mrdocs-${action}-test-fixtures-all
        COMMAND
            mrdocs-golden-tests
            --action=${action}
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
            "--addons=${CMAKE_SOURCE_DIR}/data/mrdocs/addons"
            "--stdlib-includes=${LIBCXX_DIR}"
            "--libc-includes=${CMAKE_SOURCE_DIR}/data/mrdocs/headers/libc-stubs"
            --log-level=warn
        DEPENDS mrdocs-golden-tests
    )
endforeach ()

# XML lint: validate the golden XML fixtures against the RELAX NG schema
# generated from mrdocs.rnc (via trang).
if (MRDOCS_BUILD_STRICT_TESTS)
    find_package(LibXml2 REQUIRED)
else ()
    find_package(LibXml2)
endif()
if (LibXml2_FOUND)
    find_package(Java REQUIRED Runtime)
    if (NOT Java_FOUND)
        message(FATAL_ERROR "Java is needed to run xml-lint")
    endif()

    add_custom_command(
        COMMAND ${Java_JAVA_EXECUTABLE} -jar ${PROJECT_SOURCE_DIR}/utils/codegen/trang.jar
            ${PROJECT_SOURCE_DIR}/docs/modules/ROOT/attachments/schemas/mrdocs.rnc ${CMAKE_CURRENT_BINARY_DIR}/mrdocs.rng
        OUTPUT mrdocs.rng
        DEPENDS ${PROJECT_SOURCE_DIR}/docs/modules/ROOT/attachments/schemas/mrdocs.rnc)
    add_custom_target(mrdocs_rng ALL DEPENDS mrdocs.rng)

    file(GLOB_RECURSE XML_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/*.xml)
    add_test(NAME xml-lint
             COMMAND ${LIBXML2_XMLLINT_EXECUTABLE} --dropdtd --noout
             --relaxng ${CMAKE_CURRENT_BINARY_DIR}/mrdocs.rng ${XML_SOURCES}
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
