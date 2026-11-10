#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Lists the symbols the mrdocs executable exports and checks that they are the
# MrDocs API and nothing else. Every exported symbol has to demangle to a name
# in the mrdocs namespace. That includes the type information, vtables, thunks
# and guard variables of its types, and its static locals. Anything else, such
# as a symbol of LLVM, Clang, Lua or JerryScript, fails the check and appears in
# the list. A plugin that bundles its own copy of one of those libraries has to
# bind to its own copy, which it can only do if the executable does not export
# one.
#
# Inputs: NM (the symbol lister), CXXFILT (c++filt or llvm-cxxfilt), EXE (the
# executable).

cmake_minimum_required(VERSION 3.13)

if (APPLE)
    set(nm_args -gU)
else ()
    set(nm_args --dynamic --defined-only)
endif ()

# nm piped into the demangler: one demangled symbol per line.
function(list_exports out_listing out_failed)
    execute_process(
        COMMAND "${NM}" ${ARGN} "${EXE}"
        COMMAND "${CXXFILT}"
        OUTPUT_VARIABLE listing
        RESULT_VARIABLE results
        ERROR_VARIABLE errors)
    set(failed "")
    foreach (result IN LISTS results)
        if (NOT result EQUAL 0)
            set(failed "${errors}")
        endif ()
    endforeach ()
    set(${out_listing} "${listing}" PARENT_SCOPE)
    set(${out_failed} "${failed}" PARENT_SCOPE)
    set(last_command "${NM};${ARGN};${EXE}" PARENT_SCOPE)
    set(last_errors "${errors}" PARENT_SCOPE)
endfunction()

# The symbol versions are what tell a symbol of another object apart (see
# below).
set(failed "")
if (NOT APPLE)
    # GNU nm before 2.35 prints them only with --with-symbol-versions, which
    # llvm-nm does not accept, since it prints them anyway.
    list_exports(listing failed ${nm_args} --with-symbol-versions)
endif ()
if (APPLE OR failed)
    list_exports(listing failed ${nm_args})
endif ()
if (failed)
    message(FATAL_ERROR "listing the exports of ${EXE} failed: ${failed}")
endif ()

string(REPLACE "\n" ";" lines "${listing}")
set(exported "")
foreach (line IN LISTS lines)
    # "<address> <type> <name>", the type being a letter. A name with an @ is
    # a symbol of another object, such as a libstdc++ data symbol the
    # executable has a copy relocation for. The exception is @@Base, which GNU
    # nm before 2.35 appends to the symbols of the executable itself.
    if (line MATCHES "^[0-9a-fA-F]* +[A-Za-z] +([^@]+)(@@Base)?$")
        set(symbol "${CMAKE_MATCH_1}")
        # The header Mach-O executables define for the dynamic loader, the
        # glibc marker `_IO_stdin_used`, the symbols the linker defines for
        # the ends of the sections and the startup symbols every glibc
        # executable defines (`_start`, `data_start`, `__libc_csu_init`) are
        # not MrDocs's.
        if (symbol MATCHES "^(_*(mh_execute_header|init|fini|edata|end|bss_start|bss_end|IO_stdin_used|data_start|libc_csu_init|libc_csu_fini)_*|_start)$")
            continue()
        endif ()
        # clang links its coverage runtime (`__llvm_profile_*`) statically
        # into the executable, the way the sanitizer runtimes are linked on
        # sanitized builds.
        if (symbol MATCHES "^__llvm_")
            continue()
        endif ()
        list(APPEND exported "${symbol}")
    endif ()
endforeach ()
list(LENGTH exported count)
message(STATUS "${EXE} exports ${count} symbols")
# The table has to hold at least five hundred MrDocs symbols. The libraries are
# compiled with hidden visibility and the MRDOCS_DECL markers export the API, so
# a regression that hides it would otherwise pass with an almost empty table.
set(minimum_mrdocs_symbols 500)
if (count EQUAL 0)
    # The command that produced the listing, its first lines and what it
    # wrote to stderr, so the failure shows what the lister printed. The lines
    # are cut with string operations, since a list would split at every `;`
    # and read `[` as a bracket.
    list(JOIN last_command " " last_command_text)
    set(rest "${listing}")
    set(listing_head "")
    foreach (i RANGE 1 20)
        if (rest STREQUAL "")
            break()
        endif ()
        string(FIND "${rest}" "\n" eol)
        if (eol EQUAL -1)
            string(APPEND listing_head "${rest}\n")
            set(rest "")
        else ()
            math(EXPR next "${eol} + 1")
            string(SUBSTRING "${rest}" 0 ${next} line)
            string(APPEND listing_head "${line}")
            string(SUBSTRING "${rest}" ${next} -1 rest)
        endif ()
    endforeach ()
    if (listing_head STREQUAL "")
        set(listing_head "(the output was empty)\n")
    endif ()
    set(details "command: ${last_command_text}\n")
    if (NOT "${CXXFILT}" STREQUAL "")
        string(APPEND details "piped into: ${CXXFILT}\n")
    endif ()
    if (NOT last_errors STREQUAL "")
        string(APPEND details "stderr: ${last_errors}\n")
    endif ()
    message(FATAL_ERROR
        "no exported symbol found in ${EXE}: the listing could not be read\n"
        "${details}"
        "first lines of its output (up to 20):\n${listing_head}")
endif ()

set(typeinfo_count 0)
set(mrdocs_count 0)
set(stdlib_count 0)
set(foreign "")
foreach (name IN LISTS exported)
    if (name MATCHES "^typeinfo for mrdocs::")
        math(EXPR typeinfo_count "${typeinfo_count} + 1")
    endif ()
    # A function template may carry `decltype(auto)` in front of its name.
    string(REPLACE "decltype(auto) " "" bare "${name}")
    # A std function template may also carry a `decltype (...)` return type
    # whose expression holds parentheses, so it is cut at the balanced one.
    if (bare MATCHES "^decltype \\(")
        string(LENGTH "${bare}" bare_length)
        set(depth 0)
        set(cut 0)
        foreach (i RANGE 10 ${bare_length})
            if (i EQUAL bare_length)
                break()
            endif ()
            string(SUBSTRING "${bare}" ${i} 1 ch)
            if (ch STREQUAL "(")
                math(EXPR depth "${depth} + 1")
            elseif (ch STREQUAL ")")
                if (depth EQUAL 0)
                    math(EXPR cut "${i} + 1")
                    break()
                endif ()
                math(EXPR depth "${depth} - 1")
            endif ()
        endforeach ()
        if (cut GREATER 0)
            string(SUBSTRING "${bare}" ${cut} -1 bare)
            string(STRIP "${bare}" bare)
        endif ()
    endif ()
    # Only the name counts. The template arguments go first, since they may
    # hold parentheses such as `(__gnu_cxx::_Lock_policy)2`, and then the text
    # up to the first parenthesis is what the check looks at, so it judges a
    # std type holding a MrDocs type by the name it defines.
    set(plain "${bare}")
    while (plain MATCHES "<[^<>]*>")
        string(REGEX REPLACE "<[^<>]*>" "" plain "${plain}")
    endwhile ()
    string(REGEX REPLACE "\\(.*$" "" plain "${plain}")
    # A std function over an LLVM or Clang type could bind a plugin that
    # carries its own LLVM to the copy in the executable. Type information is
    # data, and binding to it only merges the type identity. The vtables of
    # std templates over LLVM types are let through because libstdc++ gives
    # its namespace default visibility, so -fvisibility=hidden cannot hide
    # them. The remaining risk is a plugin that makes the same instantiation
    # with its own LLVM: its vtable references bind to the copy in the
    # executable.
    set(is_lib_function FALSE)
    if ((name MATCHES "(llvm|clang)::" OR bare MATCHES "[0-9](llvm|clang)[0-9]")
            AND NOT name MATCHES "^(typeinfo|typeinfo name|vtable) for ")
        set(is_lib_function TRUE)
    endif ()
    # The namespace may follow a space, since a function template carries its
    # return type in front of the name and typeinfo, vtables and thunks a
    # description. A name the demangler could not read stays mangled, and the
    # check judges it by its mangling: the namespace is the first component of
    # the nested name.
    if (plain MATCHES "(^| )mrdocs::"
            OR bare MATCHES "^_?_Z(NK?[RO]?|T[ISVTWH]N|T[hvc][A-Za-z0-9_]*_NK?[RO]?|GVZ?NK?[RO]?|ZNK?[RO]?)6mrdocs")
        math(EXPR mrdocs_count "${mrdocs_count} + 1")
    elseif (name MATCHES "^(typeinfo|typeinfo name) for [^<>]*\\(.*mrdocs::"
            AND NOT name MATCHES "(llvm|clang)::|lua_|jerry_")
        # The type information of a function type or a function pointer type
        # over MrDocs types, such as `typeinfo for bool (mrdocs::Name const&,
        # mrdocs::Name const&)`. A `std::function` holding such a callable
        # emits it, and the compiler gives it default visibility.
        math(EXPR mrdocs_count "${mrdocs_count} + 1")
    elseif (bare MATCHES "^_?_Z(St|NK?St)" AND NOT is_lib_function)
        # A standard-library function template instantiation whose constrained
        # signature (a C++20 requires-clause) the demangler cannot read, such
        # as `_ZSt12construct_atIN6mrdocs5ErrorE...`. It is judged by its
        # mangling: the name is in namespace std.
        math(EXPR stdlib_count "${stdlib_count} + 1")
    elseif (plain MATCHES "^((vtable|typeinfo|typeinfo name|guard variable) for )?([^ ]+ )*(std|__gnu_cxx)::[^ ]*$"
            AND NOT is_lib_function)
        # On libstdc++ the table also holds standard-library template
        # instantiations (`std::` and `__gnu_cxx::`, such as
        # `std::vector<mrdocs::Info>::_M_realloc_append`), because libstdc++
        # declares its namespace with default visibility and -fvisibility=hidden
        # does not hide it. They are counted and printed, and they do not fail
        # the check. A plugin's own instantiations of the same templates also
        # have default visibility, so on Linux they may bind to the copies in
        # the executable. The standard library tag the loader checks guarantees
        # that both sides were built against libstdc++, whose instantiations of
        # the same template are interchangeable.
        math(EXPR stdlib_count "${stdlib_count} + 1")
    else ()
        list(APPEND foreign "${name}")
    endif ()
endforeach ()
message(STATUS "${mrdocs_count} exported symbols are in the mrdocs namespace")
message(STATUS "${stdlib_count} exported symbols are standard-library template instantiations")
if (mrdocs_count LESS minimum_mrdocs_symbols)
    message(SEND_ERROR "${EXE} exports ${mrdocs_count} symbols in the mrdocs namespace, fewer than ${minimum_mrdocs_symbols}: the MRDOCS_DECL markers are not exporting the API")
endif ()
message(STATUS "${typeinfo_count} of them are the type information of a mrdocs type")

# The table also has to hold the type information of a MrDocs class, which a
# plugin that derives from one needs.
if (typeinfo_count EQUAL 0)
    message(SEND_ERROR "${EXE} exports no 'typeinfo for mrdocs::' symbol")
endif ()
list(LENGTH foreign foreign_count)
if (foreign_count GREATER 0)
    list(SUBLIST foreign 0 20 sample)
    string(REPLACE ";" "\n  " sample "${sample}")
    message(SEND_ERROR "${EXE} exports ${foreign_count} symbols that are not in the mrdocs namespace, for example:\n  ${sample}")
endif ()
