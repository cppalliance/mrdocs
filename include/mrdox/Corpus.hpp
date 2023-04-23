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
#include <mrdox/Debug.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/meta/Symbols.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/Support/Mutex.h>
#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace clang {
namespace mrdox {

/** The collection of declarations in extracted form.
*/
class Corpus
{
public:
    virtual ~Corpus() = default;

    //--------------------------------------------

    /** Return the Config used to generate this corpus.
    */
    virtual
    std::shared_ptr<Config const> const&
    config() const noexcept = 0;

    /** Return the list of all uniquely identified symbols.
    */
    virtual
    std::vector<SymbolID> const&
    allSymbols() const noexcept = 0;

    /** Return the metadata for the global namespace.
    */
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Return the Info with the matching ID, or nullptr.
    */
    /** @{ */
    virtual Info* find(SymbolID const& id) noexcept = 0;
    virtual Info const* find(SymbolID const& id) const noexcept = 0;
    /** @} */

    /** Return true if an Info with the specified symbol ID exists.
    */
    bool
    exists(
        SymbolID const& id) const noexcept
    {
        return find(id) != nullptr;
    }

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T>
    T const&
    get(
        SymbolID const& id) const noexcept;

    //--------------------------------------------

    /** Base class used to visit elements of the corpus.
    */
    struct Visitor
    {
        virtual ~Visitor() = default;
        virtual bool visit(NamespaceInfo const&);
        virtual bool visit(RecordInfo const&);
        virtual bool visit(Overloads const&);
        virtual bool visit(FunctionInfo const&);
        virtual bool visit(EnumInfo const&);
        virtual bool visit(TypedefInfo const&);
    };

    /** Visit the specified symbol ID or node.
    */
    /** @{ */
    [[nodiscard]] bool visit(SymbolID id, Visitor& f) const;
    [[nodiscard]] bool visit(Scope const& I, Visitor& f) const;
    [[nodiscard]] bool visitWithOverloads(Scope const& I, Visitor& f) const;
    [[nodiscard]] bool visit(Info const& I, Visitor& f) const;
    /** @} */

    //--------------------------------------------

    /** Build metadata for a set of translation units.

        @param config A shared pointer to the configuration.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    [[nodiscard]]
    static
    llvm::Expected<std::unique_ptr<Corpus>>
    build(
        tooling::ToolExecutor& ex,
        std::shared_ptr<Config const> config,
        Reporter& R);
};

//------------------------------------------------

template<class T>
T const&
Corpus::
get(
    SymbolID const& id) const noexcept
{
    auto I = find(id);
    Assert(I != nullptr);
    auto const t = static_cast<T const*>(I);
    if constexpr(std::is_same_v<T, NamespaceInfo>)
        Assert(t->IT == InfoType::IT_namespace);
    else if constexpr(std::is_same_v<T, RecordInfo>)
        Assert(t->IT == InfoType::IT_record);
    else if constexpr(std::is_same_v<T, FunctionInfo>)
        Assert(t->IT == InfoType::IT_function);
    else if constexpr(std::is_same_v<T, EnumInfo>)
        Assert(t->IT == InfoType::IT_enum);
    else if constexpr(std::is_same_v<T, TypedefInfo>)
        Assert(t->IT == InfoType::IT_typedef);
    return *t;
}

} // mrdox
} // clang

#endif
