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

#include <mrdox/Errors.hpp>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

static
llvm::StringRef
prettyFilePath(
    llvm::StringRef fullPath,
    llvm::SmallVectorImpl<char>& temp)
{
    namespace path = llvm::sys::path;

    auto it = path::rbegin(fullPath);
    auto const end = path::rend(fullPath);
    if(it == end)
    {
        temp.clear();
        return {};
    }
    for(;;)
    {
        if( *it == "source" ||
            *it == "include")
        {
            temp.assign(
                it->data(),
                fullPath.end());
            break;
        }
        ++it;
        if(it == end)
        {
            temp.assign(
                fullPath.begin(),
                fullPath.end());
            break;
        }
    }
    path::remove_dots(temp, true);
#ifdef _WIN32
    for(auto& c : temp)
        if( c == '\\')
            c = '/';
#endif
    return llvm::StringRef(
        temp.begin(), temp.size());
}

static
std::string
toString(
    std::source_location const& loc)
{
    std::string s;
    llvm::SmallString<256> temp;
    llvm::raw_string_ostream os(s);
    os <<
        prettyFilePath(loc.file_name(), temp) <<
        "(" << loc.line() << ")";
    return s;
}

void
ErrorCode::
write(
    llvm::raw_ostream& os) const
{
    os << toString(loc_) << ": " << message() << "\n";
}

void
ErrorCode::
throwFrom(
    std::source_location const& loc) const
{
    // VFALCO we could attach loc to the exception
    throw std::system_error(ec_);
}

//------------------------------------------------
//
// Reporter
//
//------------------------------------------------

void
Reporter::
fail(
    llvm::StringRef what,
    ErrorCode const& ec)
{
    llvm::errs() <<
        what << " failed: " <<
        ec.message() << "\n" <<
        "at " << toString(ec.where()) << "\n";
}

//------------------------------------------------

bool
Reporter::
success(llvm::Error&& err)
{
    if(! err)
        return true;
    // VFALCO TODO Source file, line number, and what
    llvm::errs() << toString(std::move(err)) << "\n";
    failed_ = true;
    return false;
}

bool
Reporter::
success(std::error_code const& ec)
{
    if(! ec)
        return true;
    // VFALCO TODO Source file, line number, and what
    llvm::errs() << ec.message() << "\n";
    failed_ = true;
    return false;
}

bool
Reporter::
success(
    llvm::StringRef what,
    std::error_code const& ec)
{
    if(! ec)
        return true;
    llvm::errs() <<
        what << ": " << ec.message() << "\n";
    failed_ = true;
    return false;
}

bool
Reporter::
success(
    llvm::StringRef what,
    llvm::Error& err)
{
    if(! err)
        return true;
    llvm::errs() <<
        what << ": " << toString(std::move(err)) << "\n";
    failed_ = true;
    return false;
}

} // mrdox
} // clang
