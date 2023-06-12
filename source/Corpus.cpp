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

#include "ConfigImpl.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Report.hpp>
#include <mrdox/Metadata.hpp>
#include <cassert>

namespace clang {
namespace mrdox {

//------------------------------------------------

Corpus::~Corpus() noexcept = default;

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

/** Return the metadata for the global namespace.
*/
NamespaceInfo const&
Corpus::
globalNamespace() const noexcept
{
    return get<NamespaceInfo>(SymbolID::zero);
}

//------------------------------------------------

Corpus::
Visitor::
~Visitor() noexcept = default;

bool Corpus::Visitor::visit(NamespaceInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(RecordInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(FunctionInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(TypedefInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(EnumInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(VarInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(FieldInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(SpecializationInfo const&)
{
    return true;
}

//------------------------------------------------

bool
Corpus::
traverse(
    Visitor& f,
    Info const& I) const
{
    switch(I.Kind)
    {
    case InfoKind::Namespace:
        return f.visit(static_cast<NamespaceInfo const&>(I));
    case InfoKind::Record:
        return f.visit(static_cast<RecordInfo const&>(I));
    case InfoKind::Function:
        return f.visit(static_cast<FunctionInfo const&>(I));
    case InfoKind::Typedef:
        return f.visit(static_cast<TypedefInfo const&>(I));
    case InfoKind::Enum:
        return f.visit(static_cast<EnumInfo const&>(I));
    case InfoKind::Variable:
        return f.visit(static_cast<VarInfo const&>(I));
    case InfoKind::Field:
        return f.visit(static_cast<FieldInfo const&>(I));
    case InfoKind::Specialization:
        return f.visit(static_cast<SpecializationInfo const&>(I));
    default:
        llvm_unreachable("wrong InfoKind for visit");
    }
}

bool
Corpus::
traverse(
    Visitor& f,
    NamespaceInfo const& I) const
{
    for(auto const& id : I.Members)
        if(! this->traverse(f, id))
            return false;
    for(auto const& id : I.Specializations)
        if(! this->traverse(f, id))
            return false;
    return true;
}

bool
Corpus::
traverse(
    Visitor& f,
    RecordInfo const& I) const
{
    for(auto const& id : I.Members)
        if(! this->traverse(f, id))
            return false;
    for(auto const& id : I.Specializations)
        if(! this->traverse(f, id))
            return false;
    return true;
}

bool
Corpus::
traverse(
    Visitor& f,
    SpecializationInfo const& I) const
{
    for(auto const& id : I.Members)
        if(! traverse(f, get<Info>(id.Specialized)))
            return false;
    return true;
}

bool
Corpus::
traverse(Visitor& f, SymbolID id) const
{
    return traverse(f, get<Info>(id));
}

bool
Corpus::
traverse(
    Visitor& f,
    std::vector<SymbolID> const& R) const
{
    for(auto const& id : R)
        if(! traverse(f, get<Info>(id)))
            return false;
    return true;
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

// KRYSTIAN NOTE: temporary
std::string&
Corpus::
getFullyQualifiedName(
    const Info& I,
    std::string& temp) const
{
    temp.clear();
    for(auto const& ns_id : llvm::reverse(I.Namespace))
    {
        if(const Info* ns = find(ns_id))
            temp.append(ns->Name.data(), ns->Name.size());
        else
            temp.append("<unnamed>");

        temp.append("::");
    }
    auto s = I.extractName();
    temp.append(s.data(), s.size());
    return temp;
}


} // mrdox
} // clang
