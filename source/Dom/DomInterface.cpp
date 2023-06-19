//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomInterface.hpp>
#include <mrdox/Dom/DomSymbol.hpp>
#include <utility>

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------

template<class T, class U>
class DomTrancheList : public dom::Array
{
    std::shared_ptr<Interface> I_;
    std::span<T const*> list_;
    Corpus const& corpus_;

public:
    DomTrancheList(
        std::shared_ptr<Interface> I,
        std::span<T const*> list,
        Corpus const& corpus) noexcept
        : I_(std::move(I))
        , list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t
    length() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    get(std::size_t index) const override
    {
        if(index >= list_.size())
            return nullptr;
        return dom::create<U>(*list_[index], corpus_);
    }
};

//------------------------------------------------

class DomTranche : public dom::Object
{
    std::shared_ptr<Interface> I_;
    Interface::Tranche const& tranche_;
    Corpus const& corpus_;

public:
    DomTranche(
        std::shared_ptr<Interface> I,
        Interface::Tranche const& tranche,
        Corpus const& corpus) noexcept
        : I_(std::move(I))
        , tranche_(tranche)
        , corpus_(corpus)
    {
    }

    dom::Value
    get(std::string_view key) const override
    {
        if(key == "records")
            return dom::create<
                DomTrancheList<RecordInfo,
                    DomSymbol<RecordInfo>>>(
                        I_, tranche_.Records, corpus_);
        if(key == "functions")
            return dom::create<
                DomTrancheList<FunctionInfo,
                    DomSymbol<FunctionInfo>>>(
                        I_, tranche_.Functions, corpus_);
        if(key == "enums")
            return dom::create<
                DomTrancheList<EnumInfo,
                    DomSymbol<EnumInfo>>>(
                        I_, tranche_.Enums, corpus_);
        if(key == "types")
            return dom::create<
                DomTrancheList<TypedefInfo,
                    DomSymbol<TypedefInfo>>>(
                        I_, tranche_.Types, corpus_);
        if(key == "data")
            return dom::create<
                DomTrancheList<FieldInfo,
                    DomSymbol<FieldInfo>>>(
                        I_, tranche_.Data, corpus_);
        if(key == "staticfuncs")
            return dom::create<
                DomTrancheList<FunctionInfo,
                    DomSymbol<FunctionInfo>>>(
                        I_, tranche_.StaticFunctions, corpus_);
        if(key == "staticdata")
            return dom::create<
                DomTrancheList<VariableInfo,
                    DomSymbol<VariableInfo>>>(
                        I_, tranche_.StaticData, corpus_);
        return nullptr;
    }

    std::vector<std::string_view>
    props() const override
    {
        return {
            "records",
            "functions",
            "enums",
            "types",
            "data",
            "staticfuncs",
            "staticdata"
        };
    }
};

//------------------------------------------------

} // (anon)

DomInterface::
DomInterface(
    std::shared_ptr<Interface> I,
    Corpus const& corpus) noexcept
    : I_(std::move(I))
    , corpus_(corpus)
{
}

dom::Value
DomInterface::
get(std::string_view key) const
{
    if(key == "public")
        return dom::create<DomTranche>(
            I_, I_->Public, corpus_);
    if(key == "protected")
        return dom::create<DomTranche>(
            I_, I_->Protected, corpus_);
    if(key == "private")
        return dom::create<DomTranche>(
            I_, I_->Private, corpus_);  
    return nullptr;
}

auto
DomInterface::
props() const ->
    std::vector<std::string_view> 
{
    return { "public", "protected", "private" };
};

} // mrdox
} // clang
