#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# #############################
#        PROCESS
# #############################
#
# MRDOCS_BUNDLE_CXX_RUNTIME bundles the C++ runtime for the Linux release
# package. This file does four things, in this order:
#
# 1. It stops the configure unless MRDOCS_INSTALL is ON, the build is a Linux
#    build with static MrDocs libraries and no -static in the flags, CMake is
#    3.19 or newer, and CMAKE_INSTALL_LIBDIR is `lib`.
# 2. At the end of the configure, it checks that the `mrdocs` executable exports
#    only the MrDocs API (mrdocs_check_release_host_visibility).
# 3. It sets the RPATH to $ORIGIN/../lib, written as a RUNPATH, so the
#    executable loads the bundled copies first.
# 4. At install time, it asks the installed bin/mrdocs for its runtime
#    dependencies, installs each one except glibc into lib/ of the package,
#    and fails the install if one is not found or if one that the compiler
#    knows resolves to a different file than the compiler's.
#
# The root CMakeLists.txt includes this file when MRDOCS_BUNDLE_CXX_RUNTIME is
# ON. A local build leaves it off, because a build from source runs on the
# machine that built it, which already has the runtime it was linked with. The
# CMake Workflow step of .github/workflows/ci-build.yml turns it on for the
# matrix entry of ci-matrix.yml that sets mrdocs-linux-release, the gcc build
# that produces the Linux package.
#
# #############################
#        POLICY
# #############################
#
# We use the policy Python calls "manylinux". The release binary may depend
# on glibc up to a floor, and on nothing else from the system except the
# C++ runtime that the package ships itself. The build runs in an old
# distribution with a current compiler. The package carries the C++
# runtime libraries of that compiler in lib/. A checker rejects the finished
# package in any of these cases. The package is not dynamically linked against
# glibc. It refers to a newer glibc symbol, or to a GLIBC_ tag that is not a
# version (GLIBC_PRIVATE). It needs a library that is neither one of glibc nor
# one of the bundled runtime libraries. It refers to a GLIBCXX_, CXXABI_ or
# GCC_ version that the bundled libraries do not define. The checker is
# .github/scripts/linux-portability/check-linux-portability.sh, the counterpart
# of auditwheel, and
# .github/scripts/linux-portability/test-check-linux-portability.sh shows each
# rejection firing.
#
# Floor: glibc 2.31, the glibc of Ubuntu 20.04, the container the release is
# built in (.github/workflows/ci-matrix.yml). Debian 11, Fedora 32, RHEL 9
# and everything newer are covered, since glibc is backward compatible.
#
# #############################
#          MOTIVATION
# #############################
#
# Reason: plugins must load, and the binary must run on Ubuntu 20.04 and
# newer. A binary linked with -static does neither dependably. A static glibc
# cannot dlopen a plugin without the shared glibc of the same version, and a
# dynamic binary built on a recent distribution needs that distribution's
# glibc. A plugin includes the C++23 headers of MrDocs, so it needs GCC 14 or
# later. Its libstdc++ references reach GLIBCXX_3.4.33, which the libstdc++
# of Ubuntu 20.04 (GCC 10, up to GLIBCXX_3.4.28) does not define. The package
# therefore carries the libstdc++ of the compiler that built it. The
# executable loads that copy first. When it loads a plugin, the dependency of
# the plugin on libstdc++.so.6 resolves to the copy that is already loaded.
# One runtime serves both, so the plugin loads on a distribution whose own
# libstdc++ is older.
#
# Origin: manylinux (PEP 600) and auditwheel. Their policy allows a short
# whitelist of system libraries up to capped symbol versions, libstdc++ among
# them. This release allows none from the system and ships the two runtime
# libraries instead. See
# docs/modules/ROOT/pages/contribute/invariants/binary-compatibility.adoc.
#
# Do not raise the floor by updating the container to a newer Ubuntu. Raising
# it is a decision, taken together with the checker.
#
# The settings below each say what they are for.

# #############################
# 1) Refusals
# #############################
if (NOT MRDOCS_INSTALL)
    message(FATAL_ERROR
        "MRDOCS_BUNDLE_CXX_RUNTIME is for the release package and needs MRDOCS_INSTALL=ON")
endif ()
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "MRDOCS_BUNDLE_CXX_RUNTIME is for the Linux release")
endif ()
if (MRDOCS_BUILD_SHARED)
    message(FATAL_ERROR
        "MRDOCS_BUNDLE_CXX_RUNTIME is for the build with static MrDocs libraries that is released; "
        "MRDOCS_BUILD_SHARED must be OFF")
endif ()

# No -static, for the reason given at the top: -static, --static, -static-pie or
# -Wl,-static in the common flags or in the flags of any configuration.
# .github/scripts/linux-portability/check-linux-portability.sh checks the
# binary itself.
mrdocs_static_link_flags(MRDOCS_STATIC_LINK_FLAGS)
if (MRDOCS_STATIC_LINK_FLAGS)
    message(FATAL_ERROR
        "MRDOCS_BUNDLE_CXX_RUNTIME does not allow -static: a static glibc loads "
        "plugins only on a system whose shared glibc is the exact version it was "
        "linked with, and the portability of the release comes from the glibc "
        "floor and the C++ runtime shipped in lib/. See "
        "utils/cmake/release-linux.cmake")
endif ()

# #############################
# 2) The export check
# #############################

# mrdocs_check_release_host_visibility()
#
# Stop the configure when the `mrdocs` executable would export more than the
# MrDocs API.
#
# Example
#
#   cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}"
#       CALL mrdocs_check_release_host_visibility)
#
# At the end of the configure, it reads
#
#   CXX_VISIBILITY_PRESET of mrdocs                              = hidden
#   INTERFACE_MRDOCS_HIDDEN_ARCHIVES of mrdocs-hidden-archives   = jerryscript::jerry-core;...;LLVMSupport;...
#
# and the configure goes on. If mrdocs_target_plugin_host_compile_definitions
# had not run on `mrdocs`, the first value would read `visibility-NOTFOUND` and
# the configure would stop with "needs the mrdocs executable compiled with
# hidden visibility".
#
# The release exports the API and nothing else, so that a plugin that bundles
# its own LLVM or Lua binds to its own copy.
#
# Parameters
#
#   None.
#
# Preconditions
#
#   The `mrdocs` target exists, which is why the check is deferred to the end of
#   the top-level directory (`cmake_language(DEFER)`, CMake 3.19 or later), and
#   src/CMakeLists.txt has made the `mrdocs-hidden-archives` target.
#
# Guarantees
#
#   It passes only when the executable is compiled with hidden visibility, so
#   that only what MRDOCS_DECL marks is exported, and the archives of LLVM,
#   Clang, Lua and JerryScript are linked with hidden symbols.
function(mrdocs_check_release_host_visibility)
    get_target_property(visibility mrdocs CXX_VISIBILITY_PRESET)
    if (NOT visibility STREQUAL "hidden")
        message(FATAL_ERROR
            "MRDOCS_BUNDLE_CXX_RUNTIME needs the mrdocs executable compiled with "
            "hidden visibility (CXX_VISIBILITY_PRESET hidden), which "
            "mrdocs_target_plugin_host_compile_definitions sets, so that it "
            "exports only the MrDocs API. See utils/cmake/release-linux.cmake")
    endif ()
    # The host links the archives of LLVM, Clang and Lua with hidden symbols,
    # which keeps them out of the table.
    get_target_property(archives mrdocs-hidden-archives INTERFACE_MRDOCS_HIDDEN_ARCHIVES)
    if (NOT "LLVMSupport" IN_LIST archives)
        message(FATAL_ERROR
            "MRDOCS_BUNDLE_CXX_RUNTIME needs the archives of LLVM, Clang, Lua and "
            "JerryScript linked with hidden symbols, and the property "
            "INTERFACE_MRDOCS_HIDDEN_ARCHIVES of mrdocs-hidden-archives "
            "(src/CMakeLists.txt) is empty or lacks LLVMSupport: [${archives}]. "
            "See utils/cmake/release-linux.cmake")
    endif ()
endfunction()
if (CMAKE_VERSION VERSION_LESS 3.19)
    message(FATAL_ERROR
        "MRDOCS_BUNDLE_CXX_RUNTIME needs CMake 3.19 or newer, to check the "
        "visibility of the mrdocs executable once it is defined")
endif ()

# Hidden visibility narrows the dynamic symbol table of the executable to the
# MrDocs API, with the MRDOCS_DECL markers. Nothing that LLVM, Clang, Lua or the
# libraries it links define stays in the table.
# mrdocs_target_plugin_host_compile_definitions sets it, and the configuration
# fails if the host does not have it.
cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}"
    CALL mrdocs_check_release_host_visibility)
# The checker expects RUNPATH $ORIGIN/../lib and the libraries in lib/.
if (NOT CMAKE_INSTALL_BINDIR STREQUAL "bin")
    message(FATAL_ERROR
        "MRDOCS_BUNDLE_CXX_RUNTIME expects CMAKE_INSTALL_BINDIR to be \"bin\", "
        "not \"${CMAKE_INSTALL_BINDIR}\": the RUNPATH $ORIGIN/../lib is relative to it. "
        "Configure with -DCMAKE_INSTALL_BINDIR=bin")
endif ()
if (NOT CMAKE_INSTALL_LIBDIR STREQUAL "lib")
    message(FATAL_ERROR
        "MRDOCS_BUNDLE_CXX_RUNTIME expects CMAKE_INSTALL_LIBDIR to be \"lib\", "
        "not \"${CMAKE_INSTALL_LIBDIR}\": the check of the package looks for "
        "RUNPATH $ORIGIN/../lib. Configure with -DCMAKE_INSTALL_LIBDIR=lib")
endif ()

# #############################
# 3) RUNPATH
# #############################

# RUNPATH $ORIGIN/../lib is where the executable finds the bundled libraries,
# wherever the package is unpacked. It is a RUNPATH and not a hard-coded path,
# so the package is relocatable. The variable applies to the targets created
# after this file is included, which includes the mrdocs executable.
set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
# Makes the linker write a RUNPATH, which the check expects, rather than an
# RPATH, whatever the default of the linker is.
string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--enable-new-dtags")

# #############################
# 4) Install
# #############################

# mrdocs_install_cxx_runtime()
#
# Install the shared libraries that the installed `bin/mrdocs` needs, except
# glibc, into lib/ of the package.
#
# Example
#
#   cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}"
#       CALL mrdocs_install_cxx_runtime)
#
# The install log has one line per bundled library, "Bundling the C++ runtime:"
# followed by the resolved path of the library.
#
# Parameters
#
#   None.
#
# Preconditions
#
#   The rule runs after the rule that installs `mrdocs`. Install rules run in
#   the order they are declared, and the root CMakeLists.txt includes this file
#   before src/, so the call is deferred to the end of the top-level directory.
#
# Guarantees
#
#   Each dependency is installed with FOLLOW_SYMLINK_CHAIN, so the soname link
#   and the real file it points to both land in lib/, which is the name the
#   RUNPATH lookup needs. The install stops if a dependency is not found. For
#   each dependency it asks the compiler `-print-file-name=<file name>`. When
#   the compiler finds the library on its own search path and prints an
#   existing absolute path, the install stops unless that path and the
#   dependency are the same file, so the bundled runtime is the one that
#   matches the headers the compiler used. When the compiler prints the name
#   back, it does not know the library, which is bundled without that check.
function(mrdocs_install_cxx_runtime)
    # The libraries that glibc is made of are not bundled. They are the glibc
    # part of `allowed_needed` in
    # .github/scripts/linux-portability/check-linux-portability.sh, so keep the
    # two lists together.
    set(glibc_regexes
        "^libc\\.so\\.6$" "^libm\\.so\\.6$" "^libpthread\\.so\\.0$" "^libdl\\.so\\.2$"
        "^librt\\.so\\.1$" "^libutil\\.so\\.1$" "^libresolv\\.so\\.2$"
        "^libmvec\\.so\\.1$" "^libanl\\.so\\.1$" "^ld-linux.*" "^ld64.*")
    set(glibc_args "")
    foreach (regex IN LISTS glibc_regexes)
        string(APPEND glibc_args "[==[${regex}]==] ")
    endforeach ()
    string(CONFIGURE [==[
        set(mrdocs_exe "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_BINDIR@/mrdocs")
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${mrdocs_exe}"
            RESOLVED_DEPENDENCIES_VAR mrdocs_deps
            UNRESOLVED_DEPENDENCIES_VAR mrdocs_missing
            PRE_EXCLUDE_REGEXES @glibc_args@)
        if (mrdocs_missing)
            message(FATAL_ERROR
                "MRDOCS_BUNDLE_CXX_RUNTIME: cannot find ${mrdocs_missing}, "
                "which ${mrdocs_exe} needs")
        endif ()
        foreach (dep IN LISTS mrdocs_deps)
            get_filename_component(dep_name "${dep}" NAME)
            get_filename_component(dep_real "${dep}" REALPATH)
            execute_process(
                COMMAND "@CMAKE_CXX_COMPILER@" -print-file-name=${dep_name}
                OUTPUT_VARIABLE known
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE known_result)
            if (NOT known_result EQUAL 0)
                message(FATAL_ERROR
                    "MRDOCS_BUNDLE_CXX_RUNTIME: @CMAKE_CXX_COMPILER@ -print-file-name=${dep_name} failed "
                    "(exit status ${known_result}, output \"${known}\")")
            endif ()
            if (IS_ABSOLUTE "${known}" AND EXISTS "${known}")
                get_filename_component(known_real "${known}" REALPATH)
                if (NOT dep_real STREQUAL known_real)
                    message(FATAL_ERROR
                        "MRDOCS_BUNDLE_CXX_RUNTIME: ${mrdocs_exe} resolves ${dep_name} to "
                        "${dep_real}, but the compiler's is ${known_real}")
                endif ()
            endif ()
            message(STATUS "Bundling the C++ runtime: ${dep}")
            file(INSTALL "${dep}"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/@CMAKE_INSTALL_LIBDIR@"
                FOLLOW_SYMLINK_CHAIN)
        endforeach ()
    ]==] code @ONLY)
    install(CODE "${code}")
endfunction()
cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}"
    CALL mrdocs_install_cxx_runtime)
