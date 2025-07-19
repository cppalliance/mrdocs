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

#ifndef MRDOCS_LIB_TOOL_INFOSET_HPP
#define MRDOCS_LIB_TOOL_INFOSET_HPP

#include "mrdocs/Platform.hpp"
#include "mrdocs/Metadata/Info.hpp"
#include <memory>
#include <unordered_set>

namespace clang::mrdocs {

/** A hash function for Info pointers.

    The generated hash is based on the SymbolID
    of the Info object.

    This hasher is used to implement the InfoSet.
*/
struct InfoPtrHasher
{
    using is_transparent = void;

    /** Returns the hash of the Info object.

        The hash is based on the SymbolID of the Info object.

        @param I The Info object to hash.
    */
    std::size_t
    operator()(
        const std::unique_ptr<Info>& I) const;

    /** Returns the hash of the SymbolID.

        The function allows the SymbolID to be used as a key
        in an unordered container.

        @param id The SymbolID to hash.
    */
    std::size_t
    operator()(
        SymbolID const& id) const;
};

/** Equality comparison for Info pointers.

    The comparison is based on the SymbolID of the Info object.

    This equality comparison is used to implement the InfoSet.
*/
struct InfoPtrEqual
{
    using is_transparent = void;

    /** Returns `true` if the Info objects are equal.

        The comparison is based on the SymbolID of the Info object.

        @param a The first Info object to compare.
        @param b The second Info object to compare.
    */
    bool
    operator()(
        const std::unique_ptr<Info>& a,
        const std::unique_ptr<Info>& b) const;

    /** Returns `true` if the id of the Info object is equal to the SymbolID.

        The comparison is based on the SymbolID of the Info object.

        The function allows the SymbolID to be used as a key
        in an unordered container.

        @param a The Info object to compare.
        @param b The SymbolID to compare.
    */
    bool
    operator()(
        const std::unique_ptr<Info>& a,
        SymbolID const& b) const;

    /** Returns `true` if the SymbolID is equal to the id of the Info object.

        The comparison is based on the SymbolID of the Info object.

        The function allows the SymbolID to be used as a key
        in an unordered container.

        @param a The SymbolID to compare.
        @param b The Info object to compare.
    */
    bool
    operator()(
        SymbolID const& a,
        const std::unique_ptr<Info>& b) const;
};

/** A set of Info objects.

    This set is used to store the results of the execution
    of a tool at the end of the processing.

    This is a set of unique pointers to Info objects.
*/
using InfoSet = std::unordered_set<
    std::unique_ptr<Info>, InfoPtrHasher, InfoPtrEqual>;

struct UndocumentedInfo final : SourceInfo {
    SymbolID id;
    std::string name;
    InfoKind kind;

    constexpr
    UndocumentedInfo(
        SymbolID id_,
        std::string name_,
        InfoKind kind_) noexcept
        : SourceInfo()
        , id(id_)
        , name(std::move(name_))
        , kind(kind_)
    {
    }
};

struct UndocumentedInfoHasher {
    using is_transparent = void;

    std::size_t
    operator()(SymbolID const& I) const {
        return std::hash<SymbolID>()(I);
    }

    std::size_t
    operator()(UndocumentedInfo const& I) const {
        return std::hash<SymbolID>()(I.id);
    }
};

struct UndocumentedInfoEqual {
    using is_transparent = void;

    bool
    operator()(
        UndocumentedInfo const& a,
        UndocumentedInfo const& b) const
    {
        return a.id == b.id;
    }

    bool
    operator()(
        UndocumentedInfo const& a,
        SymbolID const& b) const
    {
        return a.id == b;
    }

    bool
    operator()(
        SymbolID const& a,
        UndocumentedInfo const& b) const
    {
        return a == b.id;
    }
};

using UndocumentedInfoSet = std::unordered_set<
    UndocumentedInfo, UndocumentedInfoHasher, UndocumentedInfoEqual>;

} // clang::mrdocs

#endif
