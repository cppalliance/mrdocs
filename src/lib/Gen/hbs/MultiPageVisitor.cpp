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
#include "VisitorHelpers.hpp"
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <fstream>

namespace clang::mrdocs::hbs {

template <std::derived_from<Info> T>
void
MultiPageVisitor::
operator()(T const& I)
{
    ex_.async([this, &I](Builder& builder)
    {
        if (shouldGenerate(I, corpus_.config))
        {
            // ===================================
            // Open the output file
            // ===================================
            std::string const path = files::appendPath(outputPath_, builder.domCorpus.getURL(I));
            std::string const dir = files::getParentDir(path);
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

            count_.fetch_add(1, std::memory_order_relaxed);
        }

        // ===================================
        // Traverse the symbol members
        // ===================================
        Corpus::TraverseOptions opts = {.skipInherited = std::same_as<T, RecordInfo>};
        corpus_.traverse(opts, I, *this);
    });
}

#define INFO(T) template void MultiPageVisitor::operator()<T##Info>(T##Info const&);
#include <mrdocs/Metadata/Info/InfoNodes.inc>

} // clang::mrdocs::hbs
