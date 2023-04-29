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

#include <mrdox/Platform.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Debug.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/Metadata/Symbols.hpp>
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
class MRDOX_VISIBLE
    Corpus
{
public:
    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Corpus() noexcept;

    //--------------------------------------------

    /** Return the Config used to generate this corpus.
    */
    MRDOX_DECL
    virtual
    std::shared_ptr<Config const> const&
    config() const noexcept = 0;

    /** Return a sorted index of all 
    */
    MRDOX_DECL
    virtual
    std::vector<Info const*> const&
    index() const noexcept = 0;

    /** Return the metadata for the global namespace.
    */
    MRDOX_DECL
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Return the Info with the matching ID, or nullptr.
    */
    /** @{ */
    MRDOX_DECL virtual Info* find(SymbolID const& id) noexcept = 0;
    MRDOX_DECL virtual Info const* find(SymbolID const& id) const noexcept = 0;
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
        MRDOX_DECL virtual ~Visitor() noexcept;
        MRDOX_DECL virtual bool visit(NamespaceInfo const&);
        MRDOX_DECL virtual bool visit(RecordInfo const&);
        MRDOX_DECL virtual bool visit(Overloads const&);
        MRDOX_DECL virtual bool visit(FunctionInfo const&);
        MRDOX_DECL virtual bool visit(TypedefInfo const&);
        MRDOX_DECL virtual bool visit(EnumInfo const&);
        MRDOX_DECL virtual bool visit(VariableInfo const&);
    };

    /** Visit the specified symbol ID or node.
    */
    /** @{ */
    MRDOX_DECL bool visit(SymbolID id, Visitor& f) const;
    MRDOX_DECL bool visit(std::vector<Reference> const& R, Visitor& f) const;
    MRDOX_DECL bool visit(std::vector<SymbolID> const& R, Visitor& f) const;
    MRDOX_DECL bool visit(Scope const& I, Visitor& f) const;
    MRDOX_DECL bool visitWithOverloads(Scope const& I, Visitor& f) const;
    MRDOX_DECL bool visit(Info const& I, Visitor& f) const;
    /** @} */

    //--------------------------------------------

    /** Build metadata for a set of translation units.

        @param config A shared pointer to the configuration.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    MRDOX_DECL
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
    else if constexpr(std::is_same_v<T, TypedefInfo>)
        Assert(t->IT == InfoType::IT_typedef);
    else if constexpr(std::is_same_v<T, EnumInfo>)
        Assert(t->IT == InfoType::IT_enum);
    else if constexpr(std::is_same_v<T, VariableInfo>)
        Assert(t->IT == InfoType::IT_variable);
    return *t;
}

} // mrdox
} // clang

#endif
