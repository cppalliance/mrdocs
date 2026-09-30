#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# mrdocs_add_example: the one way an example is exercised by the build.
#
#   mrdocs_add_example(
#       DIRECTORY <dir>
#       [NAME <name>]
#       [CONFIG <file>] [ARGS <arg>...]
#       [SCRIPT <file>]
#       OUTPUTS <committed>[=<generated>]... | RUN_ONLY
#       [WILL_FAIL])
#
# DIRECTORY is the example, relative to the calling CMakeLists.txt. The
# example runs there, because several examples read files relative to the
# run, and writes into a directory of its own in the build tree. By default
# the run is the mrdocs executable, with `--config=<CONFIG>` if CONFIG is
# given, then ARGS, the output directory and the built-in directory options.
# SCRIPT runs a Python script of the example's instead, which receives
# `--output=<dir>` and the built-in directory options and finds mrdocs
# through the MRDOCS environment variable; such an example is skipped where
# no Python interpreter is found.
#
# OUTPUTS lists the files the example commits, relative to DIRECTORY. The
# test `mrdocs-example-<name>` fails if the run writes any of them
# differently, and the target `mrdocs-update-example-<name>` (and, for all
# examples at once, `mrdocs-update-examples`) rewrites them from the run.
# Where the run names a file differently from the committed copy, write
# `<committed>=<generated>`. An example that commits no output says
# RUN_ONLY instead: its test only checks that the run succeeds. WILL_FAIL
# marks a run whose failure is the expected result.
#
# NAME defaults to DIRECTORY's path under examples/, with `-` for `/`.

include_guard(GLOBAL)

get_filename_component(MRDOCS_EXAMPLES_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(MRDOCS_RUN_EXAMPLE "${CMAKE_CURRENT_LIST_DIR}/run-example.cmake")
find_package(Python3 COMPONENTS Interpreter QUIET)

# The command that runs one example into `out_dir`, as a list.
function(_mrdocs_example_command out_var dir out_dir)
    if (EX_SCRIPT)
        set(command "${Python3_EXECUTABLE}" "${dir}/${EX_SCRIPT}")
    else ()
        set(command "$<TARGET_FILE:mrdocs>")
        if (EX_CONFIG)
            list(APPEND command "--config=${EX_CONFIG}")
        endif ()
    endif ()
    list(APPEND command ${EX_ARGS} "--output=${out_dir}" ${MRDOCS_BUILTIN_DIR_ARGS})
    set(${out_var} "${command}" PARENT_SCOPE)
endfunction()

# The `cmake -P run-example.cmake` invocation, in the given MODE.
function(_mrdocs_example_driver out_var mode name dir out_dir command)
    string(REPLACE ";" "$<SEMICOLON>" command "${command}")
    string(REPLACE ";" "$<SEMICOLON>" outputs "${EX_OUTPUTS}")
    set(${out_var}
        "${CMAKE_COMMAND}" "-DMODE=${mode}" "-DNAME=${name}"
        "-DEXAMPLE_DIR=${dir}" "-DOUTPUT_DIR=${out_dir}"
        "-DMRDOCS=$<TARGET_FILE:mrdocs>" "-DCOMMAND=${command}"
        "-DOUTPUTS=${outputs}" "-DUPDATE_TARGET=mrdocs-update-example-${name}"
        -P "${MRDOCS_RUN_EXAMPLE}"
        PARENT_SCOPE)
endfunction()

# The update target for one example, gathered under mrdocs-update-examples.
function(_mrdocs_example_update_target name driver)
    add_custom_target(mrdocs-update-example-${name}
        COMMAND ${driver}
        COMMENT "Updating the committed output of example ${name}"
        VERBATIM)
    add_dependencies(mrdocs-update-example-${name} mrdocs)
    if (NOT TARGET mrdocs-update-examples)
        add_custom_target(mrdocs-update-examples)
    endif ()
    add_dependencies(mrdocs-update-examples mrdocs-update-example-${name})
endfunction()

function(mrdocs_add_example)
    cmake_parse_arguments(PARSE_ARGV 0 EX "RUN_ONLY;WILL_FAIL"
        "NAME;DIRECTORY;CONFIG;SCRIPT" "ARGS;OUTPUTS")
    if (NOT EX_OUTPUTS AND NOT EX_RUN_ONLY)
        message(FATAL_ERROR "mrdocs_add_example: give OUTPUTS or RUN_ONLY")
    endif ()
    get_filename_component(dir "${EX_DIRECTORY}" ABSOLUTE)
    set(name "${EX_NAME}")
    if (NOT name)
        file(RELATIVE_PATH name "${MRDOCS_EXAMPLES_DIR}" "${dir}")
        string(REPLACE "/" "-" name "${name}")
    endif ()
    if (EX_SCRIPT AND NOT Python3_Interpreter_FOUND)
        message(STATUS "Example ${name} needs Python and is skipped")
    else ()
        set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}")
        _mrdocs_example_command(command "${dir}" "${out_dir}")
        _mrdocs_example_driver(check check ${name} "${dir}" "${out_dir}" "${command}")
        add_test(NAME mrdocs-example-${name} COMMAND ${check})
        if (EX_WILL_FAIL)
            set_tests_properties(mrdocs-example-${name} PROPERTIES WILL_FAIL TRUE)
        endif ()
        if (EX_OUTPUTS)
            _mrdocs_example_driver(update update ${name} "${dir}" "${out_dir}" "${command}")
            _mrdocs_example_update_target(${name} "${update}")
        endif ()
    endif ()
endfunction()
