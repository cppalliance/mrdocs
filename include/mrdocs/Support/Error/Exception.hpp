//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ERROR_EXCEPTION_HPP
#define MRDOCS_API_SUPPORT_ERROR_EXCEPTION_HPP

#include <mrdocs/Support/Error/Error.hpp>
#include <exception>
#include <utility>

namespace mrdocs {

/** Type of all exceptions thrown by the API.
*/
class MRDOCS_DECL
    Exception final : public std::exception
{
    Error err_;

public:
    /** Constructor.
    */
    explicit
    Exception(
        Error err) noexcept
        : err_(std::move(err))
    {
    }

    /** Return the Error stored in the exception.
    */
    Error const&
    error() const noexcept
    {
        return err_;
    }

    /** Return a null-terminated error string.
    */
    char const*
    what() const noexcept override
    {
        return err_.message().c_str();
    }
};

} // namespace mrdocs

#endif
