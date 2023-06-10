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

#ifndef MRDOX_API_CORPUS_HPP
#define MRDOX_API_CORPUS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Namespace.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Symbols.hpp>
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
protected:
    explicit
    Corpus(
        Config const& config_) noexcept
        : config(config_)
    {
    }

public:
    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Corpus() noexcept;

    //--------------------------------------------

    /** The configuration used to generate this corpus.
    */
    Config const& config;

    /** Return a sorted index of all symbols.
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
    MRDOX_DECL
    virtual
    Info const*
    find(SymbolID const& id) const noexcept = 0;

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
        MRDOX_DECL virtual bool visit(FunctionInfo const&);
        MRDOX_DECL virtual bool visit(TypedefInfo const&);
        MRDOX_DECL virtual bool visit(EnumInfo const&);
        MRDOX_DECL virtual bool visit(VarInfo const&);
        MRDOX_DECL virtual bool visit(FieldInfo const&);
        MRDOX_DECL virtual bool visit(SpecializationInfo const&);

        MRDOX_DECL virtual bool visit(MemberEnum const&, Access);
        MRDOX_DECL virtual bool visit(MemberFunction const&, Access);
        MRDOX_DECL virtual bool visit(MemberRecord const&, Access);
        MRDOX_DECL virtual bool visit(MemberType const&, Access);
        MRDOX_DECL virtual bool visit(DataMember const&, Access);
        MRDOX_DECL virtual bool visit(StaticDataMember const&, Access);
    };

    /** Traverse the symbol, list, or its children.
    */
    /** @{ */
    MRDOX_DECL bool traverse(Visitor&, Info const& I) const;
    MRDOX_DECL bool traverse(Visitor&, NamespaceInfo const& I) const;
    MRDOX_DECL bool traverse(Visitor&, RecordInfo const& I) const;
    MRDOX_DECL bool traverse(Visitor&, SpecializationInfo const& I) const;
    MRDOX_DECL bool traverse(Visitor&, SymbolID id) const;
    MRDOX_DECL bool traverse(Visitor&, std::vector<Reference> const& R) const;
    MRDOX_DECL bool traverse(Visitor&, std::vector<SymbolID> const& R) const;
    /** @} */

    template<class F>
    bool
    traverse(NamespaceInfo const& I, F&& f)
    {
        for(auto const& ref : I.Children.Namespaces)
            if(! visit(get<NamespaceInfo>(ref.id), f))
                return false;
        for(auto const& ref : I.Children.Records)
            if(! visit(get<RecordInfo>(ref.id), f))
                return false;
        for(auto const& ref : I.Children.Functions)
            if(! visit(get<FunctionInfo>(ref.id), f))
                return false;
        for(auto const& ref : I.Children.Typedefs)
            if(! visit(get<TypedefInfo>(ref.id), f))
                return false;
        for(auto const& ref : I.Children.Enums)
            if(! visit(get<EnumInfo>(ref.id), f))
                return false;
        for(auto const& ref : I.Children.Vars)
            if(! visit(get<VarInfo>(ref.id), f))
                return false;
        for(auto const& ref : I.Children.Specializations)
            if(! visit(*get<SpecializationInfo>(ref.id), f))
                return false;
        return true;
    }

    template<class F>
    bool
    traverse(RecordInfo const& I, F&& f)
    {
        if constexpr(std::is_invocable_v<F, RecordInfo const&>)
            for(auto const& ref : I.Members.Records)
                if(! f(get<RecordInfo>(ref.id)))
                    return false;
        if constexpr(std::is_invocable_v<F, FunctionInfo const&>)
            for(auto const& ref : I.Members.Functions)
                if(! f(get<FunctionInfo>(ref.id)))
                    return false;
        if constexpr(std::is_invocable_v<F, EnumInfo const&>)
            for(auto const& ref : I.Members.Enums)
                if(! f(get<EnumInfo>(ref.id)))
                    return false;
        if constexpr(std::is_invocable_v<F, TypedefInfo const&>)
            for(auto const& ref : I.Members.Types)
                if(! f(get<TypedefInfo>(ref.id)))
                    return false;
        if constexpr(std::is_invocable_v<F, FieldInfo const&>)
            for(auto const& ref : I.Members.Fields)
                if(! f(get<FieldInfo>(ref.id)))
                    return false;
        if constexpr(std::is_invocable_v<F, VarInfo const&>)
            for(auto const& ref : I.Members.Vars)
                if(! f(get<VarInfo>(ref.id)))
                    return false;
        if constexpr(std::is_invocable_v<F, SpecializationInfo const&>)
            for(auto const& ref : I.Members.Specializations)
                if(! f(get<SpecializationInfo>(ref.id)))
                    return false;
        return true;
    }

    //--------------------------------------------

    // KRYSTIAN NOTE: temporary
    MRDOX_DECL
    std::string&
    getFullyQualifiedName(
        const Info& I,
        std::string& temp) const;
};

//------------------------------------------------

template<class T>
T const&
Corpus::
get(
    SymbolID const& id) const noexcept
{
    auto I = find(id);
    assert(I != nullptr);
    auto const t = static_cast<T const*>(I);
    if constexpr(std::is_same_v<T, NamespaceInfo>)
        assert(t->Kind == InfoKind::Namespace);
    else if constexpr(std::is_same_v<T, RecordInfo>)
        assert(t->Kind == InfoKind::Record);
    else if constexpr(std::is_same_v<T, FunctionInfo>)
        assert(t->Kind == InfoKind::Function);
    else if constexpr(std::is_same_v<T, TypedefInfo>)
        assert(t->Kind == InfoKind::Typedef);
    else if constexpr(std::is_same_v<T, EnumInfo>)
        assert(t->Kind == InfoKind::Enum);
    else if constexpr(std::is_same_v<T, VarInfo>)
        assert(t->Kind == InfoKind::Variable);
    else if constexpr(std::is_same_v<T, FieldInfo>)
        assert(t->Kind == InfoKind::Field);
    else if constexpr(std::is_same_v<T, SpecializationInfo>)
        assert(t->Kind == InfoKind::Specialization);
    return *t;
}

} // mrdox
} // clang

#endif
