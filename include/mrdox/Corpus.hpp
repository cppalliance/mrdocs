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

#include "Index.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/Support/Mutex.h>
#include <type_traits>

namespace clang {
namespace mrdox {

struct Info;
struct NamespaceInfo;
struct RecordInfo;
struct FunctionInfo;
struct EnumInfo;
struct TypedefInfo;

/** The collection of declarations in extracted form.
*/
class Corpus
{
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

        @par R The diagnostic reporting object to
        use for delivering errors and information.
    */
    [[nodiscard]]
    static
    std::unique_ptr<Corpus>
    build(
        tooling::ToolExecutor& ex,
        Config const& config,
        Reporter& R);

    /** Canonicalize the contents of the object.

        @return true upon success.

        @par R The diagnostic reporting object to
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
    T const*
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
    //--------------------------------------------
    //
    // Implementation
    //
    //--------------------------------------------

    /** Insert this element and all its children into the Corpus.

        @par Thread Safety
        May be called concurrently.
    */
    void insert(std::unique_ptr<Info> Ip);

    /** Insert Info into the index

        @par Thread Safety
        May be called concurrently.
    */
    void insertIntoIndex(Info const& I);

private:
    Corpus() = default;

    llvm::sys::Mutex infoMutex;
    llvm::sys::Mutex allSymbolsMutex;
    bool is_canonical_ = false;
};

} // mrdox
} // clang

#endif
