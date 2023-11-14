//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "MultiPageVisitor.hpp"
#include <mrdocs/Support/Path.hpp>
#include <fstream>

namespace clang {
namespace mrdocs {
namespace adoc {

void
MultiPageVisitor::
writePage(
    std::string_view text,
    std::string_view filename)
{
    std::string path = files::appendPath(outputPath_, filename);
    std::string dir = files::getParentDir(path);
    if(auto err = files::createDirectory(dir))
        err.Throw();

    std::ofstream os;
    try
    {
        os.open(path,
            std::ios_base::binary |
                std::ios_base::out |
                std::ios_base::trunc // | std::ios_base::noreplace
            );
        os.write(text.data(), text.size());
    }
    catch(std::exception const& ex)
    {
        formatError("std::ofstream(\"{}\") threw \"{}\"", path, ex.what()).Throw();
    }
}

template<class T>
void
MultiPageVisitor::
operator()(T const& I)
{
    ex_.async([this, &I](Builder& builder)
    {
        writePage(builder(I).value(),
            builder.domCorpus.getXref(I));
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
        writePage(builder(OS).value(),
            builder.domCorpus.getXref(OS));
        corpus_.traverse(OS, *this);
    });
}

#define DEFINE(T) template void \
    MultiPageVisitor::operator()<T>(T const&)

DEFINE(NamespaceInfo);
DEFINE(RecordInfo);
DEFINE(FunctionInfo);
DEFINE(EnumInfo);
DEFINE(TypedefInfo);
DEFINE(VariableInfo);
DEFINE(FieldInfo);
DEFINE(SpecializationInfo);
DEFINE(FriendInfo);
DEFINE(EnumeratorInfo);

} // adoc
} // mrdocs
} // clang
