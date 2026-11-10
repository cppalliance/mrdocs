#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Register a test that runs MrDocs with the addon root ADDONS and passes only
# if the run fails with output matching OUTPUT_REGEX.
#
#   mrdocs_add_plugin_failure_test(<name> ADDONS <addon-root> OUTPUT_REGEX <regex>)
function(mrdocs_add_plugin_failure_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "ADDONS;OUTPUT_REGEX" "")
    if (NOT arg_ADDONS OR NOT DEFINED arg_OUTPUT_REGEX)
        message(FATAL_ERROR
            "mrdocs_add_plugin_failure_test(${name}): ADDONS and OUTPUT_REGEX are required")
    endif ()
    if (arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "mrdocs_add_plugin_failure_test(${name}): unexpected arguments: "
            "${arg_UNPARSED_ARGUMENTS}")
    endif ()
    mrdocs_add_checked_test(${name}
        OUTCOME failure
        OUTPUT_REGEX "${arg_OUTPUT_REGEX}"
        ARGS
            "--config=${CMAKE_CURRENT_SOURCE_DIR}/mrdocs.yml"
            "--output=${CMAKE_CURRENT_BINARY_DIR}/${name}-output"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            "--addons-supplemental=${arg_ADDONS}")
endfunction()

# A plugin MrDocs has to refuse, built from SOURCE into an addon root of its
# own, so that it is offered to its own test and to no other. Every other
# plugin in the tree reports what its headers and compiler define, so nothing
# else reaches these paths. The test passes if the run is refused with output
# matching OUTPUT_REGEX.
#
#   mrdocs_add_refused_plugin(<name> SOURCE <source> OUTPUT_REGEX <regex>)
function(mrdocs_add_refused_plugin name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE;OUTPUT_REGEX" "")
    if (NOT arg_SOURCE OR NOT DEFINED arg_OUTPUT_REGEX)
        message(FATAL_ERROR
            "mrdocs_add_refused_plugin(${name}): SOURCE and OUTPUT_REGEX are required")
    endif ()
    if (arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "mrdocs_add_refused_plugin(${name}): unexpected arguments: "
            "${arg_UNPARSED_ARGUMENTS}")
    endif ()
    set(addons "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    mrdocs_add_plugin_module(mrdocs-plugin-${name} ${arg_SOURCE} ADDONS "${addons}")
    mrdocs_add_plugin_failure_test(mrdocs-plugin-${name}-refused
        ADDONS "${addons}"
        OUTPUT_REGEX "${arg_OUTPUT_REGEX}")
endfunction()

#########################################
# 1) The export table holds only the MrDocs API
#########################################

# A sanitized build loads plugins, and it exports more than the API: clang links
# the sanitizer runtime statically into the executable, and ENABLE_EXPORTS
# exports its entry points (__asan_*, __ubsan_*, the interceptors), which are
# not in the `mrdocs` namespace. A plugin built with the same flags binds to
# them through the executable, which is what it needs, so the only consequence
# is that `mrdocs-plugin-exports` is not registered there.
mrdocs_flags_match(MRDOCS_SANITIZED "-fsanitize=")
if (UNIX AND NOT MRDOCS_SANITIZED)
    # It runs on Linux and macOS, where the toolchain that builds MrDocs brings
    # nm and c++filt: binutils on Linux, the Command Line Tools on macOS. A
    # machine without them has a broken toolchain, so the configure stops
    # instead of dropping the test.
    find_program(MRDOCS_NM NAMES nm llvm-nm)
    if (NOT MRDOCS_NM)
        message(FATAL_ERROR
            "nm (or llvm-nm) was not found: mrdocs-plugin-exports needs it to "
            "list the symbols the tool exports")
    endif ()
    find_program(MRDOCS_CXXFILT NAMES c++filt llvm-cxxfilt)
    if (NOT MRDOCS_CXXFILT)
        message(FATAL_ERROR
            "c++filt (or llvm-cxxfilt) was not found: mrdocs-plugin-exports "
            "needs it to demangle the symbols the tool exports")
    endif ()
    # The target a plugin binds to exports what MRDOCS_DECL marks and none
    # of LLVM, Clang, Lua or JerryScript.
    add_test(NAME mrdocs-plugin-exports
        COMMAND ${CMAKE_COMMAND}
            "-DNM=${MRDOCS_NM}"
            "-DCXXFILT=${MRDOCS_CXXFILT}"
            # In a static build the target is the executable. In a shared
            # build it is the core library, and the executable exports none
            # of the API.
            "-DEXE=$<IF:$<BOOL:${MRDOCS_BUILD_SHARED}>,$<TARGET_FILE:mrdocs-core>,$<TARGET_FILE:mrdocs>>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/exports/CheckExports.cmake")
endif ()

#########################################
# 2) A plugin links and loads against the exported API
#########################################

# A module library built and linked the way a plugin is, and then loaded, so
# that both building and loading it are the test: see exports/LinkProbe.cpp for
# what that catches.
set(MRDOCS_LINK_PROBE_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/link-probe")
mrdocs_add_plugin_module(mrdocs-plugin-api-link-test exports/LinkProbe.cpp
    ADDONS "${MRDOCS_LINK_PROBE_ADDONS}" FLAT)
# The library installs nothing: it has no entry point, so the run is refused for
# lacking the build id, and that refusal is only reached if the loader resolved
# every MrDocs symbol the library names. A symbol the tool does not export fails
# earlier, with an undefined-symbol message, and the test fails.
mrdocs_add_plugin_failure_test(mrdocs-plugin-api-link-test-loads
    ADDONS "${MRDOCS_LINK_PROBE_ADDONS}"
    OUTPUT_REGEX "does not export mrdocs_plugin_build_id")

#########################################
# 3) A plugin built with other standard library settings is refused
#########################################

# A plugin built with standard library settings other than the ones MrDocs was
# built with stops at its compile, since mrdocs::plugin-api gives it the tag of
# MrDocs. The target is left out of the default build, and the test builds it and
# expects the compiler to refuse it with the message of Plugin.hpp. The setting
# is chosen by library. The MSVC standard library has no check at compile time.
if (MRDOCS_STDLIB_TAG_DEFINITIONS)
    set(MRDOCS_STDLIB_MISMATCH)
    # When the configurations have different tags, the text names each one with
    # its configuration, and no one setting fails them all, so there is no test.
    if (MRDOCS_STDLIB_TAG MATCHES ": ")
        message(STATUS "The standard library tag differs between configurations: no mismatch test")
    elseif (MRDOCS_STDLIB_TAG MATCHES "libstdc\\+\\+, cxx11-abi 1")
        set(MRDOCS_STDLIB_MISMATCH -D_GLIBCXX_USE_CXX11_ABI=0)
    elseif (MRDOCS_STDLIB_TAG MATCHES "libstdc\\+\\+, cxx11-abi 0")
        set(MRDOCS_STDLIB_MISMATCH -D_GLIBCXX_USE_CXX11_ABI=1)
    elseif (MRDOCS_STDLIB_TAG MATCHES "libc\\+\\+")
        # libc++ sets its ABI version in __config_site, whatever the command
        # line says, so the plugin cannot be built with another one. The tag
        # MrDocs expects is replaced instead.
        set(MRDOCS_STDLIB_MISMATCH
            -UMRDOCS_EXPECTED_STDLIB_TAG
            "-DMRDOCS_EXPECTED_STDLIB_TAG=\"libc++, abi 0\"")
    endif ()
    if (MRDOCS_STDLIB_MISMATCH)
        mrdocs_add_plugin_module(mrdocs-plugin-stdlib-mismatch refused/NotAPlugin.cpp)
        set_target_properties(mrdocs-plugin-stdlib-mismatch PROPERTIES
            EXCLUDE_FROM_ALL ON)
        # Options follow the definitions on the compile line.
        target_compile_options(mrdocs-plugin-stdlib-mismatch PRIVATE
            ${MRDOCS_STDLIB_MISMATCH})
        mrdocs_add_checked_test(mrdocs-plugin-stdlib-mismatch-refused
            OUTCOME build-failure
            OUTPUT_REGEX "the standard library settings of this plugin differ"
            COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}"
                --target mrdocs-plugin-stdlib-mismatch --config $<CONFIG>)
        # It builds in this tree, which no other test may be running from.
        set_tests_properties(mrdocs-plugin-stdlib-mismatch-refused PROPERTIES
            RUN_SERIAL TRUE)
    endif ()
endif ()

#########################################
# 4) A plugin with a foreign build id, tag or entry point is refused
#########################################

mrdocs_add_refused_plugin(foreign-build-id
    SOURCE refused/ForeignBuildId.cpp
    OUTPUT_REGEX "was built for MrDocs build 0123456789ab, and this is build ")
mrdocs_add_refused_plugin(foreign-tag
    SOURCE refused/ForeignTag.cpp
    OUTPUT_REGEX "was built with foreign-stdlib, and this MrDocs with ")
mrdocs_add_refused_plugin(null-build-id
    SOURCE refused/NullBuildId.cpp
    OUTPUT_REGEX "reports no build id")
mrdocs_add_refused_plugin(null-tag
    SOURCE refused/NullTag.cpp
    OUTPUT_REGEX "reports no standard library")
mrdocs_add_refused_plugin(missing-main
    SOURCE refused/MissingMain.cpp
    OUTPUT_REGEX "does not export mrdocs_plugin_main")

# An entry point that fails with a message: the run names the reason.
mrdocs_add_refused_plugin(main-error
    SOURCE refused/MainError.cpp
    OUTPUT_REGEX "the plugin \".*mrdocs-plugin-main-error.*\" failed: the entry point gave up")
# An entry point that fails without one: the run only says the plugin failed.
mrdocs_add_refused_plugin(main-false
    SOURCE refused/MainFalse.cpp
    OUTPUT_REGEX "the plugin \".*mrdocs-plugin-main-false.*\" reported a failure")

# A library with none of the entry points, like a dependency of a plugin
# that someone put directly in the extensions directory instead of in the
# directory of the plugin that uses it.
mrdocs_add_refused_plugin(not-a-plugin
    SOURCE refused/NotAPlugin.cpp
    OUTPUT_REGEX "the plugin \".*mrdocs-plugin-not-a-plugin.*\" does not export mrdocs_plugin_build_id")

#########################################
# 5) Two generators with one id are refused
#########################################

# The same generator id in two plugins.
set(MRDOCS_DUPLICATE_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/duplicate-generator")
foreach (tag a b)
    mrdocs_add_plugin_module(mrdocs-plugin-duplicate-${tag} refused/IdGenerator.cpp
        ADDONS "${MRDOCS_DUPLICATE_ADDONS}")
    target_compile_definitions(mrdocs-plugin-duplicate-${tag} PRIVATE
        "MRDOCS_TEST_GENERATOR_ID=\"duplicate\"")
endforeach ()
# The one that loads second is refused. The libraries load in name order.
mrdocs_add_plugin_failure_test(mrdocs-plugin-duplicate-generator-refused
    ADDONS "${MRDOCS_DUPLICATE_ADDONS}"
    OUTPUT_REGEX "the plugin \".*mrdocs-plugin-duplicate-b.*\" failed: generator id=\"duplicate\" already exists")

# A script and a plugin that install a generator with one id.
set(MRDOCS_SCRIPT_FIRST_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/script-then-native")
file(WRITE "${MRDOCS_SCRIPT_FIRST_ADDONS}/extensions/a.lua"
    "mrdocs.register_generator(\"duplicate\", function(ctx) end)\n")
mrdocs_add_plugin_module(mrdocs-plugin-script-then-native refused/IdGenerator.cpp
    ADDONS "${MRDOCS_SCRIPT_FIRST_ADDONS}")
target_compile_definitions(mrdocs-plugin-script-then-native PRIVATE
    "MRDOCS_TEST_GENERATOR_ID=\"duplicate\"")
# Extensions load in the order of their names, and the one that comes second is
# refused: `a.lua` sorts before the directory of the plugin.
mrdocs_add_plugin_failure_test(mrdocs-plugin-script-then-native-refused
    ADDONS "${MRDOCS_SCRIPT_FIRST_ADDONS}"
    OUTPUT_REGEX "the plugin \".*mrdocs-plugin-script-then-native.*\" failed: generator id=\"duplicate\" already exists")

# A script that comes after the plugin is refused as well: an id belongs to
# one generator, whoever registers it.
set(MRDOCS_NATIVE_FIRST_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/native-then-script")
file(WRITE "${MRDOCS_NATIVE_FIRST_ADDONS}/extensions/z.lua"
    "mrdocs.register_generator(\"duplicate\", function(ctx) end)\n")
mrdocs_add_plugin_module(mrdocs-plugin-native-then-script refused/IdGenerator.cpp
    ADDONS "${MRDOCS_NATIVE_FIRST_ADDONS}")
target_compile_definitions(mrdocs-plugin-native-then-script PRIVATE
    "MRDOCS_TEST_GENERATOR_ID=\"duplicate\"")
# The directory of the plugin sorts before `z.lua`.
mrdocs_add_plugin_failure_test(mrdocs-plugin-native-then-script-refused
    ADDONS "${MRDOCS_NATIVE_FIRST_ADDONS}"
    OUTPUT_REGEX "the script \".*z\\.lua\" failed: generator id=\"duplicate\" already exists")

# A generator id that ships with MrDocs.
mrdocs_add_refused_plugin(builtin-generator
    SOURCE refused/IdGenerator.cpp
    OUTPUT_REGEX "the plugin \".*mrdocs-plugin-builtin-generator.*\" failed: generator id=\"xml\" already exists")
target_compile_definitions(mrdocs-plugin-builtin-generator PRIVATE
    "MRDOCS_TEST_GENERATOR_ID=\"xml\"")

#########################################
# 6) Files that cannot be loaded
#########################################

# An extensions entry that is a regular file, not a directory.
set(MRDOCS_FILE_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/extensions-is-a-file")
file(MAKE_DIRECTORY "${MRDOCS_FILE_ADDONS}")
file(WRITE "${MRDOCS_FILE_ADDONS}/extensions" "not a directory\n")
# It holds no extension, so the run goes on after saying the directory could not
# be read.
mrdocs_add_checked_test(mrdocs-plugin-extensions-is-a-file
    OUTCOME success
    OUTPUT_REGEX "The extensions directory \".*extensions-is-a-file/extensions\" could not be read"
    ARGS
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/project/mrdocs.yml"
        --generator=noop
        "--output=${CMAKE_CURRENT_BINARY_DIR}/mrdocs-plugin-extensions-is-a-file-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${MRDOCS_FILE_ADDONS}")

# Creating a link needs a privilege on Windows, so the case is left out there.
if (NOT WIN32)
    set(MRDOCS_DANGLING_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/dangling-link")
    file(MAKE_DIRECTORY "${MRDOCS_DANGLING_ADDONS}/extensions")
    # A symbolic link in the extensions directory whose target is missing, as
    # one left by a build tree that was moved or cleaned.
    file(CREATE_LINK "no-such-directory/missing-plugin.so"
        "${MRDOCS_DANGLING_ADDONS}/extensions/mrdocs-plugin-dangling.so" SYMBOLIC)
    # It has a library name, so it is a plugin that cannot be loaded and the run
    # names it.
    mrdocs_add_plugin_failure_test(mrdocs-plugin-dangling-link-refused
        ADDONS "${MRDOCS_DANGLING_ADDONS}"
        OUTPUT_REGEX "the plugin \".*mrdocs-plugin-dangling\\.so\" could not be loaded")
endif ()

# A Windows link cannot leave a symbol undefined, so the library cannot be built
# there.
if (NOT WIN32)
    # The refusal to load a library that refers to a symbol nothing provides.
    # The loader resolves symbols when it opens a library, so the failure has to
    # name the symbol at load time instead of crashing at the first call.
    mrdocs_add_refused_plugin(undefined-symbol
        SOURCE refused/UndefinedSymbol.cpp
        OUTPUT_REGEX "could not be loaded:.*mrdocs_test_symbol_nothing_provides")
    if (APPLE)
        target_link_options(mrdocs-plugin-undefined-symbol PRIVATE
            -undefined dynamic_lookup)
    endif ()
endif ()

#########################################
# 7) The shared libraries a plugin depends on
#########################################

# The plugin dependencies/DependentPlugin.cpp calls a helper library, and fails
# unless the helper returns its value.
#
# On Windows, the loader does not search PATH for the helper.
if (WIN32)
    set(dependent_addons "${CMAKE_CURRENT_BINARY_DIR}/dependent-addons")
    set(dependency_helper_dir "${CMAKE_CURRENT_BINARY_DIR}/dependency-helper")

    add_library(mrdocs-plugin-dependency-helper SHARED dependencies/DependencyHelper.cpp)
    # The helper lives in a directory that is not the extensions directory, not
    # the directory of the plugin and not the one holding the executable.
    set_target_properties(mrdocs-plugin-dependency-helper PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<1:${dependency_helper_dir}>")

    mrdocs_add_plugin_module(mrdocs-plugin-dependent dependencies/DependentPlugin.cpp
        ADDONS "${dependent_addons}")
    target_link_libraries(mrdocs-plugin-dependent
        PRIVATE mrdocs-plugin-dependency-helper)

    # Plain LoadLibrary would find the helper in PATH and load the plugin. The
    # loader's search flags leave PATH out, so the plugin has to be refused at
    # load time. The test matches the "could not be loaded" message and not the
    # localized system text. This also runs the loader's error reporting on
    # Windows.
    mrdocs_add_plugin_failure_test(mrdocs-plugin-dependency-not-on-path-refused
        ADDONS "${dependent_addons}"
        OUTPUT_REGEX "the plugin \".*mrdocs-plugin-dependent.*\" could not be loaded:")
    # The directory of the helper is put in PATH.
    file(TO_NATIVE_PATH "${dependency_helper_dir}" native_helper_dir)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
        set_tests_properties(mrdocs-plugin-dependency-not-on-path-refused PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${native_helper_dir}")
    else ()
        set(path_with_helper "$ENV{PATH}")
        string(REPLACE ";" "\\;" path_with_helper
            "${native_helper_dir};${path_with_helper}")
        set_tests_properties(mrdocs-plugin-dependency-not-on-path-refused PROPERTIES
            ENVIRONMENT "PATH=${path_with_helper}")
    endif ()
endif ()

# Every platform: the helper sits next to the plugin in its directory under
# `extensions`, where the loader takes only the entry file for a plugin, and
# has to be found.
set(MRDOCS_FOUND_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/found-addons")
set(MRDOCS_FOUND_STEM mrdocs-plugin-dependent-found)

add_library(mrdocs-plugin-dependency-helper-found SHARED dependencies/DependencyHelper.cpp)
mrdocs_hide_symbols(mrdocs-plugin-dependency-helper-found)
set_target_properties(mrdocs-plugin-dependency-helper-found PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY
        "$<1:${MRDOCS_FOUND_ADDONS}/extensions/${MRDOCS_FOUND_STEM}>"
    RUNTIME_OUTPUT_DIRECTORY
        "$<1:${MRDOCS_FOUND_ADDONS}/extensions/${MRDOCS_FOUND_STEM}>")

# The build rpath that CMake would add, naming the build directory, is turned
# off.
mrdocs_add_plugin_module(${MRDOCS_FOUND_STEM} dependencies/DependentPlugin.cpp
    ADDONS "${MRDOCS_FOUND_ADDONS}")
# On Linux and macOS the plugin finds the library through its own rpath, which
# is the only search path it has. On Windows the loader adds the directory of the
# plugin to the directories it searches while it loads the plugin.
target_link_libraries(${MRDOCS_FOUND_STEM} PRIVATE
    mrdocs-plugin-dependency-helper-found)
# The name of the plugin file has no prefix, so that it is `<name>/<name>` on
# every platform.
set_target_properties(${MRDOCS_FOUND_STEM} PROPERTIES PREFIX "")

# The entry point of the plugin fails unless the helper returns its value, so a
# run that loads the plugin shows the helper was found and called.
mrdocs_add_checked_test(mrdocs-plugin-dependency-next-to-entry
    OUTCOME success
    OUTPUT_REGEX "Loaded plugin \".*${MRDOCS_FOUND_STEM}"
    ARGS
        "--config=${CMAKE_CURRENT_SOURCE_DIR}/project/mrdocs.yml"
        --generator=noop
        "--output=${CMAKE_CURRENT_BINARY_DIR}/mrdocs-plugin-dependency-next-to-entry-output"
        ${MRDOCS_BUILTIN_DIR_ARGS}
        "--addons-supplemental=${MRDOCS_FOUND_ADDONS}"
        --log-level=info)

# The directory flags go to the script with | between them, since a ; would
# split the -D definition.
string(REPLACE ";" "|" MRDOCS_NON_ASCII_BUILTIN_ARGS "${MRDOCS_BUILTIN_DIR_ARGS}")
# The same plugin and helper, copied to an addon root whose name is not ASCII:
# on Windows, a path read in the ANSI code page names another directory. The
# run has to load the plugin and find its helper, as the one above does.
add_test(NAME mrdocs-plugin-dependency-non-ascii-root
    COMMAND ${CMAKE_COMMAND}
        "-DMRDOCS=$<TARGET_FILE:mrdocs>"
        "-DFROM=${MRDOCS_FOUND_ADDONS}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/non-ascii-root"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/project"
        "-DBUILTIN_ARGS=${MRDOCS_NON_ASCII_BUILTIN_ARGS}"
        "-DRUN_CHECKED=${MRDOCS_RUN_CHECKED_SCRIPT}"
        "-DREGEX=Loaded plugin \".*${MRDOCS_FOUND_STEM}"
        # NonAsciiPath.cmake makes the copy and runs the tool, and names the
        # copy in a configuration file, which MrDocs reads as UTF-8, because on
        # Windows its command line arrives in the ANSI code page.
        -P "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/NonAsciiPath.cmake")

#########################################
# 8) One plugin loaded again and again
#########################################

# The same plugin loaded three times into one process, into two registries and
# then into the first again. The tool loads plugins once per run, so a program
# of its own does it.
set(MRDOCS_RELOAD_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/reload-addons")
add_executable(mrdocs-plugin-reload-host reload/ReloadHost.cpp)
# The program is linked the way the tool is.
mrdocs_target_plugin_host_compile_definitions(mrdocs-plugin-reload-host)
# The plugin links the program in the tool's place.
mrdocs_add_plugin_module(mrdocs-plugin-reload reload/ReloadPlugin.cpp
    ADDONS "${MRDOCS_RELOAD_ADDONS}" FLAT HOST mrdocs-plugin-reload-host)
# The plugin's entry point has to run once, and the transform it installed has
# to reach both registries.
add_test(NAME mrdocs-plugin-reload
    COMMAND mrdocs-plugin-reload-host "${MRDOCS_RELOAD_ADDONS}")

#########################################
# 9) Symbols stay inside each plugin
#########################################

# Two plugins that each export a function of the same name, which return
# different values. macOS and Windows bind names per library, so the run says
# nothing there.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MRDOCS_LOCAL_SYMBOLS_ADDONS "${CMAKE_CURRENT_BINARY_DIR}/local-symbols")
    foreach (tag a b)
        mrdocs_add_plugin_module(mrdocs-plugin-local-symbols-${tag}
            exports/LocalSymbols.cpp exports/LocalSymbolsTag.cpp
            ADDONS "${MRDOCS_LOCAL_SYMBOLS_ADDONS}")
        target_compile_definitions(mrdocs-plugin-local-symbols-${tag} PRIVATE
            "MRDOCS_TEST_TAG=\"${tag}\"")
    endforeach ()
    # The loader opens a library so that its symbols stay its own, so the entry
    # point of each one calls its own function. If the symbols of the first were
    # global, the second would call that one and be refused.
    mrdocs_add_checked_test(mrdocs-plugin-local-symbols
        OUTCOME success
        OUTPUT_REGEX "Loaded plugin \".*mrdocs-plugin-local-symbols-b"
        ARGS
            "--config=${CMAKE_CURRENT_SOURCE_DIR}/project/mrdocs.yml"
            --generator=noop
            "--output=${CMAKE_CURRENT_BINARY_DIR}/mrdocs-plugin-local-symbols-output"
            ${MRDOCS_BUILTIN_DIR_ARGS}
            "--addons-supplemental=${MRDOCS_LOCAL_SYMBOLS_ADDONS}"
            --log-level=info)
endif ()

# The messages of the refusals above.
include("${CMAKE_CURRENT_SOURCE_DIR}/refused/messages/refusal-messages.cmake")

# Every test of this directory belongs to the integration category.
get_property(mrdocs_plugin_api_tests DIRECTORY PROPERTY TESTS)
if (mrdocs_plugin_api_tests)
    set_tests_properties(${mrdocs_plugin_api_tests} PROPERTIES LABELS integration)
endif ()
