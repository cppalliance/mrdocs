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

#include <mrdox/Error.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/Support/Path.h>
#include <utility>

namespace clang {
namespace mrdox {

llvm::StringRef
nice(
    std::source_location loc)
{
    namespace path = llvm::sys::path;

    static thread_local llvm::SmallString<0> temp;

    llvm::StringRef fileName(loc.file_name());
    auto it = path::rbegin(fileName);
    auto const end = path::rend(fileName);
    if(it == end)
    {
        temp.clear();
        return {};
    }
    for(;;)
    {
        // VFALCO This assumes the directory
        //        layout of the source files.
        if( *it == "source" ||
            *it == "include")
        {
            temp.assign(
                it->data(),
                fileName.end());
            break;
        }
        ++it;
        if(it == end)
        {
            temp = fileName;
            break;
        }
    }
    path::remove_dots(temp, true);
    temp.push_back('(');
    temp.append(std::to_string(loc.line()));
    temp.push_back(')');
    return temp;
}

namespace {

class ErrorInfoPlus
    : public llvm::ErrorInfo<ErrorInfoPlus>
{
    std::string action_;
    std::source_location loc_;

public:
    static char ID;

    ErrorInfoPlus(
        std::string action,
        std::source_location loc)
        : action_(std::move(action))
        , loc_(loc)
    {
    }

    void
    log(
        llvm::raw_ostream &os) const override
    {
        os << action_ << " at " << nice(loc_);
    }

    std::error_code
    convertToErrorCode() const override
    {
        return llvm::inconvertibleErrorCode();
    }

    void const*
    dynamicClassID() const override
    {
        return this;
    }
};

char ErrorInfoPlus::ID{};

} // (anon)

llvm::Error
makeErrorString(
    std::string reason,
    std::source_location loc)
{
    return llvm::make_error<ErrorInfoPlus>(std::move(reason), loc);
}

} // mrdox
} // clang
