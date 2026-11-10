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

# Script extensions and compiled plugins, each with a project to run it on. An
# example directory keeps all its variants together and calls
# mrdocs_add_extension_example from its own CMakeLists.txt, which defines the
# example's tests. Output goes to the build tree.
#
# An example directory has the variants it needs from this list:
#
#   - plugin/plugin.cpp is the extension as a compiled plugin, built here.
#   - lua/extensions/ and js/extensions/ hold the extension as a Lua and as a
#     JavaScript script. Each directory is an addon root of its own, because
#     two scripts of one name cannot share an extensions/ directory.
#
# mrdocs_add_extension_example(<name>
#     [INPUT <dir>] [CONFIG <file>] [MODE report|output|file|xml-check]
#     [FILTER <regex>] [EXPECTED <file>] [SORT] [NO_SCRIPT] [GENERATOR <id>]
#     [OUTPUT_FILE <name>] [CHECK <script>] [PLUGIN_SOURCE <file>] [RUN_SH])
#
# Called from examples/extensions/<name>/CMakeLists.txt. Each variant the
# directory has runs alone and its result is compared with the one committed
# EXPECTED file. The tests are named mrdocs-extension-example-<name>-<variant>,
# with <variant> one of cpp, lua and js. INPUT is a directory of sources to
# document, CONFIG a configuration file, and PLUGIN_SOURCE the plugin source
# when it is not plugin/plugin.cpp. NO_SCRIPT is for an example that has only
# the plugin, and RUN_SH also adds the test ...-run-sh. MODE says what the result
# is.
#
#   report     The extension only reports. The lines the run logs that match
#              FILTER are the result. SORT is for a log whose order is not
#              fixed.
#   output     The extension changes the corpus. The reference.adoc the run
#              writes is the result.
#   file       The extension is a generator, run with `--generator` GENERATOR.
#              The file OUTPUT_FILE it writes is the result.
#   xml-check  CONFIG with the xml generator, then the CMake script CHECK on the
#              reference.xml it wrote, with XML set to that file. Only the cpp
#              variant runs, as ...-cpp, and the check is ...-cpp-check.
function (mrdocs_add_extension_example_impl name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "SORT;RUN_SH;NO_SCRIPT"
        "INPUT;CONFIG;MODE;FILTER;EXPECTED;GENERATOR;OUTPUT_FILE;CHECK;PLUGIN_SOURCE" "")
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(out "${CMAKE_CURRENT_BINARY_DIR}/output")
    set(prefix mrdocs-extension-example-${name})
    set(compare_log "${PROJECT_SOURCE_DIR}/examples/extensions/CompareLog.cmake")
    set(compare_files "${PROJECT_SOURCE_DIR}/examples/extensions/CompareFiles.cmake")
    set(run_command "${PROJECT_SOURCE_DIR}/utils/cmake/helpers/RunCommand.cmake")

    # RUN_SH adds the test ...-run-sh, which runs run.sh of the example
    # directory, so that the example is run with its own mrdocs.yml.
    if (arg_RUN_SH)
        add_test(NAME ${prefix}-run-sh
            COMMAND bash run.sh
                    --output=${out}/run
                    ${MRDOCS_BUILTIN_DIR_ARGS}
            WORKING_DIRECTORY "${dir}")
        set_property(TEST ${prefix}-run-sh PROPERTY
            ENVIRONMENT "MRDOCS=$<TARGET_FILE:mrdocs>")
    endif ()
    if (NOT arg_MODE)
        return()
    endif ()

    # plugin/plugin.cpp, or PLUGIN_SOURCE.
    set(plugin_source "${dir}/plugin/plugin.cpp")
    if (arg_PLUGIN_SOURCE)
        set(plugin_source "${arg_PLUGIN_SOURCE}")
    endif ()
    set(roots)
    set(variants)
    set(scripts)
    if (EXISTS "${plugin_source}")
        # The plugin is built into an addon root of the build tree.
        set(plugin_addons "${CMAKE_CURRENT_BINARY_DIR}/addons")
        mrdocs_add_plugin(${prefix}-cpp "${plugin_source}"
            NAME ${name} ADDONS "${plugin_addons}")
        set(root_cpp "${plugin_addons}")
        list(APPEND variants cpp)
    else ()
        message(FATAL_ERROR "${name} has no plugin source ${plugin_source}")
    endif ()
    # lua/ and js/ are addon roots as they are.
    foreach (language IN ITEMS lua js)
        if (IS_DIRECTORY "${dir}/${language}/extensions")
            set(root_${language} "${dir}/${language}")
            list(APPEND variants ${language})
            list(APPEND scripts ${language})
        endif ()
    endforeach ()

    # Modes report, output and file need the lua/ and js/ roots, and stop the
    # configuration when one is missing. NO_SCRIPT is for an example that has
    # only the plugin.
    if (NOT arg_MODE STREQUAL "xml-check" AND NOT arg_NO_SCRIPT)
        foreach (language IN ITEMS lua js)
            if (NOT language IN_LIST scripts)
                message(FATAL_ERROR
                    "${dir}/${language}/extensions does not exist, and the "
                    "example ${name} does not pass NO_SCRIPT")
            endif ()
        endforeach ()
    endif ()

    # The configuration is CONFIG, or the mrdocs.yml of the example directory,
    # or, when INPUT is given, one written to the build tree for the sources of
    # INPUT with a single-page adoc as the output. One of the three is
    # required.
    if (arg_CONFIG)
        set(config "${arg_CONFIG}")
    elseif (NOT arg_INPUT AND EXISTS "${dir}/mrdocs.yml")
        set(config "${dir}/mrdocs.yml")
    elseif (NOT arg_INPUT)
        message(FATAL_ERROR
            "${name} needs CONFIG, an mrdocs.yml in ${dir}, or INPUT")
    else ()
        set(input_dir "${arg_INPUT}")
        set(config "${CMAKE_CURRENT_BINARY_DIR}/mrdocs.yml")
        file(WRITE "${config}"
            "source-root: ${input_dir}\n"
            "input:\n"
            "  - ${input_dir}\n"
            "file-patterns:\n"
            "  - ${name}.cpp\n"
            "generator: adoc\n"
            "multipage: false\n"
            "show-namespaces: false\n")
    endif ()

    # Each variant gets its own root as `--addons-supplemental`, on the same
    # configuration.
    foreach (variant IN LISTS variants)
        set(args_${variant} "--config=${config}" ${MRDOCS_BUILTIN_DIR_ARGS}
            "--output=${out}/${variant}"
            "--addons-supplemental=${root_${variant}}")
    endforeach ()

    if (arg_MODE STREQUAL "report")
        foreach (variant IN LISTS variants)
            add_test(NAME ${prefix}-${variant}
                COMMAND ${CMAKE_COMMAND} "-DFILTER=${arg_FILTER}"
                    "-DRUN_COMMAND=${run_command}"
                    "-DSORT=${arg_SORT}" "-DEXPECTED=${arg_EXPECTED}"
                    -P "${compare_log}"
                    -- $<TARGET_FILE:mrdocs> ${args_${variant}}
                       --log-level=info)
        endforeach ()
    elseif (arg_MODE STREQUAL "output")
        foreach (variant IN LISTS variants)
            add_test(NAME ${prefix}-${variant}
                COMMAND ${CMAKE_COMMAND}
                    "-DRUN_COMMAND=${run_command}"
                    "-DEXPECTED=${arg_EXPECTED}"
                    "-DACTUAL=${out}/${variant}/reference.adoc"
                    -P "${compare_files}"
                    -- $<TARGET_FILE:mrdocs> ${args_${variant}}
                       --log-level=warn)
        endforeach ()
    elseif (arg_MODE STREQUAL "file")
        foreach (variant IN LISTS variants)
            add_test(NAME ${prefix}-${variant}
                COMMAND ${CMAKE_COMMAND}
                    "-DRUN_COMMAND=${run_command}"
                    "-DEXPECTED=${arg_EXPECTED}"
                    "-DACTUAL=${out}/${variant}/${arg_OUTPUT_FILE}"
                    -P "${compare_files}"
                    -- $<TARGET_FILE:mrdocs> ${args_${variant}}
                       --generator=${arg_GENERATOR} --log-level=warn)
        endforeach ()
    elseif (arg_MODE STREQUAL "xml-check")
        add_test(NAME ${prefix}-cpp
            COMMAND mrdocs "--config=${config}" --generator=xml
                "--output=${out}/cpp" ${MRDOCS_BUILTIN_DIR_ARGS}
                "--addons-supplemental=${root_cpp}" --log-level=warn)
        set_tests_properties(${prefix}-cpp PROPERTIES
            FIXTURES_SETUP ${prefix}-cpp)
        add_test(NAME ${prefix}-cpp-check
            COMMAND ${CMAKE_COMMAND} "-DXML=${out}/cpp/reference.xml"
                -P "${arg_CHECK}")
        set_tests_properties(${prefix}-cpp-check PROPERTIES
            FIXTURES_REQUIRED ${prefix}-cpp)
    else ()
        message(FATAL_ERROR "unknown MODE ${arg_MODE} for ${name}")
    endif ()
endfunction ()

function (mrdocs_add_extension_example name)
    get_property(before DIRECTORY PROPERTY TESTS)
    mrdocs_add_extension_example_impl(${name} ${ARGN})
    get_property(after DIRECTORY PROPERTY TESTS)
    if (before)
        list(REMOVE_ITEM after ${before})
    endif ()
    if (after)
        # Every test is labeled `integration`.
        set_tests_properties(${after} PROPERTIES LABELS integration)
    endif ()
endfunction ()

# Each directory defines its own tests, with mrdocs_add_extension_example or
# written out for a plugin that has a project of its own to run on.
add_subdirectory(brief-filler)
add_subdirectory(brief-from-name)
add_subdirectory(count-by-kind)
add_subdirectory(deprecations)
add_subdirectory(ghostwriter)
add_subdirectory(hello-world-generator)
add_subdirectory(hello-world-transform)
add_subdirectory(parse-format-relates)
add_subdirectory(project-conventions)
add_subdirectory(subclass-tree)
