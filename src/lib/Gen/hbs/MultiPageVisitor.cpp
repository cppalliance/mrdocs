//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "MultiPageVisitor.hpp"
#include <mrdocs/Support/Path.hpp>
#include <fstream>

namespace clang {
namespace mrdocs {
namespace hbs {

void
MultiPageVisitor::
writePage(
    std::string_view text,
    std::string_view filename)
{
    std::string path = files::appendPath(outputPath_, filename);
    std::string dir = files::getParentDir(path);
    auto exp = files::createDirectory(dir);
    if (!exp)
    {
        exp.error().Throw();
    }
    std::ofstream os;
    try
    {
        os.open(path,
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc // | std::ios_base::noreplace
            );
        os.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    catch(std::exception const& ex)
    {
        formatError(R"(std::ofstream("{}") threw "{}")", path, ex.what()).Throw();
    }
}

template<std::derived_from<Info> T>
void
MultiPageVisitor::
operator()(T const& I)
{
    ex_.async([this, &I](Builder& builder)
    {
        if(const auto r = builder(I))
            writePage(*r, builder.domCorpus.getXref(I));
        else
            r.error().Throw();
        if constexpr(
                T::isNamespace() ||
                T::isRecord() ||
                T::isEnum())
        {
            // corpus_.traverse(I, *this);
            corpus_.traverseOverloads(I, *this);
        }
    });
}

void
MultiPageVisitor::
operator()(OverloadSet const& OS)
{
    ex_.async([this, OS](Builder& builder)
    {
        if(const auto r = builder(OS))
            writePage(*r, builder.domCorpus.getXref(OS));
        else
            r.error().Throw();
        corpus_.traverse(OS, *this);
    });
}

#define DEFINE(T) template void \
    MultiPageVisitor::operator()<T>(T const&)

#define INFO(Type) DEFINE(Type##Info);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

} // hbs
} // mrdocs
} // clang
