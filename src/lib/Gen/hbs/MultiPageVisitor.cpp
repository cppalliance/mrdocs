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

template <class T>
requires std::derived_from<T, Info> || std::same_as<T, OverloadSet>
void
MultiPageVisitor::
operator()(T const& I0)
{
    // If T is an OverloadSet, we make a copy for the lambda because
    // these are temporary objects that don't live in the corpus.
    // Otherwise, the lambda will capture a reference to the corpus Info.
    auto Ref = [&I0] {
        if constexpr (std::derived_from<T, Info>)
        {
            return std::ref(I0);
        }
        else if constexpr (std::same_as<T, OverloadSet>)
        {
            return OverloadSet(I0);
        }
    }();
    ex_.async([this, Ref](Builder& builder)
    {
        T const& I = Ref;

        // ===================================
        // Open the output file
        // ===================================
        std::string path = files::appendPath(outputPath_, builder.domCorpus.getXref(I));
        std::string dir = files::getParentDir(path);
        if (auto exp = files::createDirectory(dir); !exp)
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
            if (!os.is_open()) {
                formatError(R"(std::ofstream("{}") failed)", path)
                    .Throw();
            }
        }
        catch (std::exception const& ex)
        {
            formatError(R"(std::ofstream("{}") threw "{}")", path, ex.what())
                .Throw();
        }

        // ===================================
        // Generate the output
        // ===================================
        if (auto exp = builder(os, I); !exp)
        {
            exp.error().Throw();
        }

        // ===================================
        // Traverse the symbol members
        // ===================================
        if constexpr (std::derived_from<T, Info>)
        {
            if constexpr(
                T::isNamespace() ||
                T::isRecord() ||
                T::isEnum())
            {
                corpus_.traverseOverloads(I, *this);
            }
        }
        else if constexpr (std::same_as<T, OverloadSet>)
        {
            corpus_.traverse(I, *this);
        }
    });
}

#define INFO(T) template void MultiPageVisitor::operator()<T##Info>(T##Info const&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

template void MultiPageVisitor::operator()<OverloadSet>(OverloadSet const&);

} // hbs
} // mrdocs
} // clang
