//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <lib/Dom/LazyArray.hpp>
#include <lib/Dom/LazyObject.hpp>
#include <span>

namespace clang {
namespace mrdocs {

dom::String toString(NameKind kind) noexcept
{
    switch(kind)
    {
    case NameKind::Identifier:
        return "identifier";
    case NameKind::Specialization:
        return "specialization";
    default:
        MRDOCS_UNREACHABLE();
    }
}

static
void
writeTo(
    std::string& result,
    auto&&... args)
{
    (result += ... += args);
}

static
void
toStringImpl(
    std::string& result,
    const NameInfo& N)
{
    if(N.Prefix)
    {
        toStringImpl(result, *N.Prefix);
        writeTo(result, "::");
    }

    writeTo(result, N.Name);

    if(! N.isSpecialization())
        return;
    std::span<const std::unique_ptr<TArg>> targs =
        static_cast<const SpecializationNameInfo&>(N).TemplateArgs;
    writeTo(result, '<');
    if(! targs.empty())
    {
        auto targ_writer =
            [&]<typename U>(const U& u)
            {
                if constexpr(U::isType())
                {
                    if(u.Type)
                        writeTo(result, toString(*u.Type));
                }
                if constexpr(U::isNonType())
                {
                    writeTo(result, u.Value.Written);
                }
                if constexpr(U::isTemplate())
                {
                    writeTo(result, u.Name);
                }
                if(u.IsPackExpansion)
                    writeTo(result, "...");
            };
        visit(*targs.front(), targ_writer);
        for(const auto& arg : targs.subspan(1))
        {
            writeTo(result, ", ");
            visit(*arg, targ_writer);
        }
    }
    writeTo(result, '>');
}

std::string
toString(const NameInfo& N)
{
    std::string result;
    toStringImpl(result, N);
    return result;
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NameInfo const& I,
    DomCorpus const* domCorpus)
{
    io.map("kind", I.Kind);
    visit(I, [domCorpus, &io]<typename T>(const T& t)
    {
        io.map("name", t.Name);
        io.map("symbol", t.id);
        if constexpr(requires { t.TemplateArgs; })
        {
            io.map("args", dom::LazyArray(t.TemplateArgs, domCorpus));
        }
        io.map("prefix", t.Prefix);
    });
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NameInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs
} // clang
