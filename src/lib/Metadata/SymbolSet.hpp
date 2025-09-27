//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_SYMBOLSET_HPP
#define MRDOCS_LIB_METADATA_SYMBOLSET_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <memory>
#include <unordered_set>

namespace mrdocs {

/** A hash function for Symbol pointers.

    The generated hash is based on the SymbolID
    of the Symbol object.

    This hasher is used to implement the SymbolSet.
*/
struct SymbolPtrHasher
{
    using is_transparent = void;

    /** Returns the hash of the Symbol object.

        The hash is based on the SymbolID of the Symbol object.

        @param I The Symbol object to hash.
    */
    std::size_t
    operator()(
        const std::unique_ptr<Symbol>& I) const;

    /** Returns the hash of the SymbolID.

        The function allows the SymbolID to be used as a key
        in an unordered container.

        @param id The SymbolID to hash.
    */
    std::size_t
    operator()(
        SymbolID const& id) const;
};

/** Equality comparison for Symbol pointers.

    The comparison is based on the SymbolID of the Symbol object.

    This equality comparison is used to implement the SymbolSet.
*/
struct SymbolPtrEqual
{
    using is_transparent = void;

    /** Returns `true` if the Symbol objects are equal.

        The comparison is based on the SymbolID of the Symbol object.

        @param a The first Symbol object to compare.
        @param b The second Symbol object to compare.
    */
    bool
    operator()(
        const std::unique_ptr<Symbol>& a,
        const std::unique_ptr<Symbol>& b) const;

    /** Returns `true` if the id of the Symbol object is equal to the SymbolID.

        The comparison is based on the SymbolID of the Symbol object.

        The function allows the SymbolID to be used as a key
        in an unordered container.

        @param a The Symbol object to compare.
        @param b The SymbolID to compare.
    */
    bool
    operator()(
        const std::unique_ptr<Symbol>& a,
        SymbolID const& b) const;

    /** Returns `true` if the SymbolID is equal to the id of the Symbol object.

        The comparison is based on the SymbolID of the Symbol object.

        The function allows the SymbolID to be used as a key
        in an unordered container.

        @param a The SymbolID to compare.
        @param b The Symbol object to compare.
    */
    bool
    operator()(
        SymbolID const& a,
        const std::unique_ptr<Symbol>& b) const;
};

/** A set of Symbol objects.

    This set is used to store the results of the execution
    of a tool at the end of the processing.

    This is a set of unique pointers to Symbol objects.
*/
using SymbolSet = std::unordered_set<
    std::unique_ptr<Symbol>, SymbolPtrHasher, SymbolPtrEqual>;

struct UndocumentedSymbol final {
    SymbolID id;
    std::string name;
    SymbolKind kind;
    SourceInfo Loc;

    constexpr
    UndocumentedSymbol(
        SymbolID id_,
        std::string name_,
        SymbolKind kind_) noexcept
        : id(id_)
        , name(std::move(name_))
        , kind(kind_)
    {
    }
};

struct UndocumentedSymbolHasher {
    using is_transparent = void;

    std::size_t
    operator()(SymbolID const& I) const {
        return std::hash<SymbolID>()(I);
    }

    std::size_t
    operator()(UndocumentedSymbol const& I) const {
        return std::hash<SymbolID>()(I.id);
    }
};

struct UndocumentedSymbolEqual {
    using is_transparent = void;

    bool
    operator()(
        UndocumentedSymbol const& a,
        UndocumentedSymbol const& b) const
    {
        return a.id == b.id;
    }

    bool
    operator()(
        UndocumentedSymbol const& a,
        SymbolID const& b) const
    {
        return a.id == b;
    }

    bool
    operator()(
        SymbolID const& a,
        UndocumentedSymbol const& b) const
    {
        return a == b.id;
    }
};

using UndocumentedSymbolSet = std::unordered_set<
    UndocumentedSymbol, UndocumentedSymbolHasher, UndocumentedSymbolEqual>;

} // mrdocs

#endif // MRDOCS_LIB_METADATA_SYMBOLSET_HPP
