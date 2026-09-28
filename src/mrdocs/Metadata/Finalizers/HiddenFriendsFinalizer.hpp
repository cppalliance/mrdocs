//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_HIDDENFRIENDSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_HIDDENFRIENDSFINALIZER_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/detail/Corpus.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mrdocs {

/** Surfaces the hidden friends of base classes on derived classes.

    A hidden friend is found only through argument-dependent lookup on the
    class that declares it or on a class derived from it. As
    `inherit-hidden-friends` directs, this finalizer either references a
    base's hidden friend from the derived class or copies it: a copy is a
    namespace-scope function placed in the nearest documented ancestor
    namespace of the derived class, with the base type in its parameters
    rewritten to the derived type and a `relates` entry naming the derived
    class. The hidden friends of a class are found through the
    `HiddenFriendOf` back-link on function symbols, so the pass does not
    depend on `extract-friends` recording the friendship on the class.
*/
class HiddenFriendsFinalizer
{
    Corpus& corpus_;
    Config const& config_;
    std::unordered_set<SymbolID> finalized_;

    // The hidden friends each record declares, indexed from the
    // `HiddenFriendOf` back-link of every function in the corpus, in a
    // deterministic order.
    std::unordered_map<SymbolID, std::vector<SymbolID>> hiddenFriends_;

    void
    indexHiddenFriends();

    // One hidden friend a record surfaces for its own derived classes: the
    // original friend, the base id substituted into a copied signature, the
    // emitted copy (`SymbolID::invalid` when the friend was referenced
    // rather than copied), and the class that declares the friend as
    // written in the base-specifier that reached it. Inherited through
    // this map rather than through `RecordSymbol::Friends`, which lists
    // only the friends a class itself declares, so inheritance stays
    // transitive.
    struct InheritedHiddenFriend
    {
        SymbolID friendId;
        SymbolID fromId;
        SymbolID copyId;
        SymbolID declaringId;
    };
    std::unordered_map<SymbolID, std::vector<InheritedHiddenFriend>>
        inheritedHiddenFriends_;

    void
    inheritHiddenFriends(
        RecordSymbol const& I,
        RecordSymbol const& B,
        SymbolID const& fromId,
        AccessKind A,
        SourceInfo const& baseLoc);

public:
    HiddenFriendsFinalizer(
        Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    void
    build();

    void
    operator()(RecordSymbol& I);

};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_HIDDENFRIENDSFINALIZER_HPP
