//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "MultiPageVisitor.hpp"
#include <mrdox/Support/Path.hpp>
#include <fstream>

namespace clang {
namespace mrdox {
namespace adoc {

template<class T>
void
MultiPageVisitor::
operator()(T const& I)
{
    renderPage(I);
    if constexpr(
            T::isNamespace() ||
            T::isRecord())
        corpus_.traverse(I, *this);
}

void
MultiPageVisitor::
renderPage(
    auto const& I)
{
    ex_.async(
        [this, &I](Builder& builder)
        {
            auto pageText = builder(I).value();


            std::string fileName = files::appendPath(
                outputPath_, builder.domCorpus.getXref(I.id) + ".adoc");

            std::string dir = files::getParentDir(fileName);
            if(auto err = files::createDirectory(dir))
                err.Throw();

            std::ofstream os;
            try
            {
                os.open(fileName,
                    std::ios_base::binary |
                        std::ios_base::out |
                        std::ios_base::trunc // | std::ios_base::noreplace
                    );
                os.write(pageText.data(), pageText.size());
            }
            catch(std::exception const& ex)
            {
                formatError("std::ofstream(\"{}\") threw \"{}\"", fileName, ex.what()).Throw();
            }
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

} // adoc
} // mrdox
} // clang
