//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_CLIOVERRIDE_HPP
#define MRDOCS_LIB_SUPPORT_CLIOVERRIDE_HPP

#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Support/Error.hpp>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace mrdocs {

/** Parse a command-line scalar the way a YAML scalar resolves.

    `true`/`false` become a Boolean, a base-10 integer becomes an Integer,
    and anything else stays a String. This keeps a value set on the command
    line type-compatible with the same value written in the configuration
    file (for example, `multipage` must read back as a Boolean).
*/
dom::Value
parseCliScalarValue(std::string_view value);

/** Whether `arg` is a dotted override for one of the named object options.

    Matches a token of the form `--<name>.<rest>` (optionally `=value`)
    where `<name>` is one of `objectOptionNames`. These tokens address
    nested keys that have no fixed command-line flag, so they are removed
    from the argument list before the registered options are parsed and
    applied separately by @ref applyDottedObjectOverrides.
*/
bool
isDottedObjectOverride(
    std::string_view arg,
    std::span<std::string_view const> objectOptionNames);

/** Apply `--<optionName>.<key>.<path...>=<value>` overrides onto a map.

    For each matching token in `argv`, the first dotted segment after
    `optionName` selects (or creates) the map entry, and the remaining
    segments address a nested field within that entry's object, created on
    demand. Values already loaded from the configuration file are kept
    unless a command-line override addresses the same field.

    @return An error if a matching token is malformed (no `=value`, or
    fewer than a key plus one field).
*/
Expected<void>
applyDottedObjectOverrides(
    std::map<std::string, dom::Object>& target,
    std::string_view optionName,
    char const** argv);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_CLIOVERRIDE_HPP
