//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_CORPUS_HPP
#define MRDOX_CORPUS_HPP

#include <mrdox/Config.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/meta/Index.hpp>
#include <mrdox/meta/Types.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/Support/Mutex.h>
#include <type_traits>
#include <vector>

namespace clang {
namespace mrdox {

/** The collection of declarations in extracted form.
*/
class Corpus
{
    Config const& config_;

    explicit
    Corpus(
        Config const& config) noexcept
        : config_(config)
    {
    }

public:
    /** Index of all emitted symbols.
    */
    Index Idx;

    /** Table of Info keyed on Symbol ID.
    */
    llvm::StringMap<std::unique_ptr<Info>> InfoMap;

    /** List of all symbols.
    */
    std::vector<SymbolID> allSymbols;

public:
    //--------------------------------------------
    //
    // Modifiers
    //
    //--------------------------------------------

    /** Build the intermediate representation of the code being documented.

        @param config The configuration, whose lifetime
        must extend until the corpus is destroyed.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    [[nodiscard]]
    static
    llvm::Expected<std::unique_ptr<Corpus>>
    build(
        tooling::ToolExecutor& ex,
        Config const& config,
        Reporter& R);

    /** Canonicalize the contents of the object.

        @return true upon success.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    [[nodiscard]]
    bool
    canonicalize(Reporter& R);

    /** Sort an array of Info by fully qualified name
    */

    /** Store the Info in the tool results, keyed by SymbolID.
    */
    static
    void
    reportResult(
        tooling::ExecutionContext& exc,
        Info const& I);

    //--------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------

    /** Return true if s0 is less than s1.

        This function returns true if the string
        s0 is less than the string s1. The comparison
        is first made without regard to case, unless
        the strings compare equal and then they
        are compared with lowercase letters coming
        before uppercase letters.
    */
    static
    bool
    symbolCompare(
        llvm::StringRef symbolName0,
        llvm::StringRef symbolName1) noexcept;

    /** Return the ID of the global namespace.
    */
    static
    SymbolID
    globalNamespaceID() noexcept;

    /** Return the metadata for the global namespace.
    */
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Return true if an Info with the specified symbol ID exists.
    */
    bool
    exists(SymbolID const& id) const noexcept
    {
        return find<Info>(id) != nullptr;
    }

    /** Return a pointer to the Info with the specified symbol ID, or nullptr.
    */
    template<class T>
    T*
    find(
        SymbolID const& id) noexcept;

    /** Return a pointer to the Info with the specified symbol ID, or nullptr.
    */
    template<class T>
    T const*
    find(
        SymbolID const& id) const noexcept;

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T>
    T&
    get(
        SymbolID const& id) noexcept
    {
        auto p = find<T>(id);
        assert(p != nullptr);
        return *p;
    }

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T>
    T const&
    get(
        SymbolID const& id) const noexcept
    {
        auto p = find<T>(id);
        assert(p != nullptr);
        return *p;
    }
    
private:
    struct Temps;

    //--------------------------------------------
    //
    // Implementation
    //
    //--------------------------------------------

    /** Insert this element and all its children into the Corpus.

        @param Thread Safety
        May be called concurrently.
    */
    void insert(std::unique_ptr<Info> Ip);

    /** Insert Info into the index

        @param Thread Safety
        May be called concurrently.
    */
    void insertIntoIndex(Info const& I);

    bool canonicalize(NamespaceInfo& I, Temps& t, Reporter& R);
    bool canonicalize(RecordInfo& I, Temps& t, Reporter& R);
    bool canonicalize(FunctionInfo& I, Temps& t, Reporter& R);
    bool canonicalize(EnumInfo& I, Temps& t, Reporter& R);
    bool canonicalize(TypedefInfo& I, Temps& t, Reporter& R);
    bool canonicalize(Scope& I, Temps& t, Reporter& R);
    bool canonicalize(std::vector<Reference>& list, Temps& t, Reporter& R);
    bool canonicalize(llvm::SmallVectorImpl<MemberTypeInfo>& list, Temps& t, Reporter& R);

private:
    llvm::sys::Mutex infoMutex;
    llvm::sys::Mutex allSymbolsMutex;
    bool isCanonical_ = false;
};

//------------------------------------------------

template<class T>
T*
Corpus::
find(
    SymbolID const& id) noexcept
{
    auto it = InfoMap.find(llvm::toStringRef(id));
    if(it != InfoMap.end())
    {
        auto const t = static_cast<T*>(it->getValue().get());
        if constexpr(std::is_same_v<T, NamespaceInfo>)
            assert(t->IT == InfoType::IT_namespace);
        else if constexpr(std::is_same_v<T, RecordInfo>)
            assert(t->IT == InfoType::IT_record);
        else if constexpr(std::is_same_v<T, FunctionInfo>)
            assert(t->IT == InfoType::IT_function);
        else if constexpr(std::is_same_v<T, EnumInfo>)
            assert(t->IT == InfoType::IT_enum);
        else if constexpr(std::is_same_v<T, TypedefInfo>)
            assert(t->IT == InfoType::IT_typedef);
        return t;
    }
    return nullptr;
}

/** Return a pointer to the Info with the specified symbol ID, or nullptr.
*/
template<class T>
T const*
Corpus::
find(
    SymbolID const& id) const noexcept
{
    auto it = InfoMap.find(llvm::toStringRef(id));
    if(it != InfoMap.end())
    {
        auto const t = static_cast<
            T const*>(it->getValue().get());
        if constexpr(std::is_same_v<T, NamespaceInfo>)
            assert(t->IT == InfoType::IT_namespace);
        else if constexpr(std::is_same_v<T, RecordInfo>)
            assert(t->IT == InfoType::IT_record);
        else if constexpr(std::is_same_v<T, FunctionInfo>)
            assert(t->IT == InfoType::IT_function);
        else if constexpr(std::is_same_v<T, EnumInfo>)
            assert(t->IT == InfoType::IT_enum);
        else if constexpr(std::is_same_v<T, TypedefInfo>)
            assert(t->IT == InfoType::IT_typedef);
        return t;
    }
    return nullptr;
}

} // mrdox
} // clang

#endif
