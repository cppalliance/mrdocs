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

#ifndef MRDOX_API_XML_XMLGENERATOR_HPP
#define MRDOX_API_XML_XMLGENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <llvm/ADT/Optional.h>

namespace clang {
namespace mrdox {
namespace xml {

//------------------------------------------------

struct XMLGenerator : Generator
{
    llvm::StringRef
    name() const noexcept override
    {
        return "xml";
    }

    llvm::StringRef
    displayName() const noexcept override
    {
        return "Extensible Markup Language (XML)";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "xml";
    }

    llvm::Error
    buildSinglePage(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R,
        llvm::raw_fd_ostream* fd_os) const override;
};

} // xml
} // mrdox
} // clang

#endif
