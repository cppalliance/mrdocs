//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_SETMEMBER_HPP
#define MRDOCS_LIB_EXTENSIONS_SETMEMBER_HPP

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>

#include <string>
#include <unordered_map>

namespace mrdocs {

class CorpusImpl;
class DomCorpus;
struct Symbol;

/** Per-script state threaded through every `mrdocs.*` callback.

    Each binding captures it differently (Lua via a light-userdata
    upvalue, JS via a raw pointer captured in a `dom::Function`
    lambda). Holds the live corpus and a string -> `Symbol*` index that
    avoids re-decoding the `SymbolID` encoding on every setter call.
*/
struct ExtensionState
{
    CorpusImpl* corpus = nullptr;
    std::unordered_map<std::string, Symbol*> byId;
};

/** Expose the canonical DOM of every symbol to scripts.

    Returns a DOM value whose `symbols` field is the array of
    per-symbol lazy objects, matching what the Handlebars generators
    see. As a side effect, populates `state.byId` so subsequent
    `setMemberImpl` calls can route back to the live C++ object by
    the same base16 `id` string the DOM exposes.
*/
dom::Value
buildCorpusDom(
    CorpusImpl& corpus,
    DomCorpus const& domCorpus,
    ExtensionState& state);

/** Language-agnostic implementation of `mrdocs.set`.

    Validates `(symbol_id, field, value)` against the allowlist and
    dispatches reflection through the dynamic symbol type. Returns
    the rich `Error` for every failure path: unknown symbol id,
    off-allowlist field, type mismatch, unknown enum name, unknown
    polymorphic kind, unknown sub-field, missing kind, and so on.

    The success value is `dom::Value()` (nil); callers that want a
    meaningful return type can ignore it.
*/
Expected<dom::Value, Error>
setMemberImpl(
    ExtensionState& state,
    dom::Value const& idArg,
    dom::Value const& fieldArg,
    dom::Value const& valueArg);

} // mrdocs

#endif
