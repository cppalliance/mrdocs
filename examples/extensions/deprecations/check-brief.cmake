#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Fail unless the XML output shows what the plugin's transform does to the
# sample project. Run with -DXML=<file>.
file(READ "${XML}" content)

# Checks the text of the element of the function with the given name, the
# first one after its name up to the closing tag of the function. Leaves the
# element in `element`.
function (check_function name expected)
    check_element(function "${name}" "${expected}")
    set(element "${element}" PARENT_SCOPE)
endfunction ()

# The same for the element with the given tag, such as `overloads`: the text
# between its name and its closing tag has to contain the brief.
function (check_element tag name expected)
    string(FIND "${content}" "<${tag}>\n  <name>${name}</name>" start)
    if (start EQUAL -1)
        message(FATAL_ERROR "the ${tag} ${name} is missing from ${XML}")
    endif ()
    string(SUBSTRING "${content}" ${start} -1 rest)
    string(FIND "${rest}" "</${tag}>" end)
    string(SUBSTRING "${rest}" 0 ${end} element)
    string(FIND "${element}" "<literal>${expected}</literal>" position)
    if (position EQUAL -1)
        message(FATAL_ERROR
            "the brief of ${name} is not \"${expected}\" in ${XML}")
    endif ()
    set(element "${element}" PARENT_SCOPE)
endfunction ()

# `resize` is deprecated and has no documentation, so its brief is the message
# of its attribute.
check_function(resize "Deprecated: slated for removal")
# A deprecated symbol with no message gets a brief that only says so.
check_function(reset "Deprecated.")
# `Label` lists a copy of `legacy`, a deprecated member of a type of another
# library, and the copy gets the brief too.
check_function(legacy "Deprecated: not ours")
# An overload set of deprecated functions with no documentation gets a brief
# for its own row in the namespace listing: the shared message of `retire`, and
# a plain "Deprecated." for `drop`, whose members differ.
check_element(overloads retire "Deprecated: use Circle::resize instead")
check_element(overloads drop "Deprecated.")

# `getRadius` is deprecated too, but its author wrote a brief, which stays.
check_function(getRadius "The old way to get the radius.")
string(FIND "${element}" "Deprecated" position)
if (NOT position EQUAL -1)
    message(FATAL_ERROR
        "the transform wrote a brief next to the one the author wrote, in ${XML}")
endif ()

# The transform leaves alone the symbols the documentation does not show: a
# hidden type in a `detail` namespace, and a type of another library that is
# only a dependency.
foreach (hidden IN ITEMS "Deprecated: internal" "Deprecated: not ours either")
    string(FIND "${content}" "${hidden}" position)
    if (NOT position EQUAL -1)
        message(FATAL_ERROR
            "the transform wrote \"${hidden}\" for a symbol that is not shown, in ${XML}")
    endif ()
endforeach ()

string(FIND "${content}" "Deprecated: use the radius member instead" position)
if (NOT position EQUAL -1)
    message(FATAL_ERROR
        "the transform replaced a brief the author wrote, in ${XML}")
endif ()
