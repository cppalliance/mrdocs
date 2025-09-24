//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_GENERATORS_HPP
#define MRDOCS_API_GENERATORS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>
#include <cstdint>
#include <string_view>

namespace clang {
namespace mrdocs {

/** A dynamic list of @ref Generator elements.
*/
class MRDOCS_VISIBLE
    Generators
{
protected:
    Generators() noexcept = default;

public:
    using value_type = Generator const*;
    using iterator = value_type const*;
    using const_iterator = iterator;
    using reference = value_type const&;
    using const_reference = value_type const&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    /** Destructor.
    */
    MRDOCS_DECL
    virtual
    ~Generators() noexcept;

    /** Return an iterator to the beginning.
    */
    MRDOCS_DECL
    virtual
    iterator
    begin() const noexcept = 0;

    /** Return an iterator to the end.
    */
    MRDOCS_DECL
    virtual
    iterator
    end() const noexcept = 0;

    /** Return a pointer to the matching generator.

        @return A pointer to the generator, or `nullptr`.

        @param name The name of the generator. The
        name must be an exact match, including case.
    */
    MRDOCS_DECL
    virtual
    Generator const*
    find(
        std::string_view name) const noexcept = 0;
};

/** Return a reference to the global Generators instance.
*/
MRDOCS_DECL
Generators const&
getGenerators() noexcept;

} // mrdocs
} // clang

#endif // MRDOCS_API_GENERATORS_HPP
