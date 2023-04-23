//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_GENERATORS_HPP
#define MRDOX_GENERATORS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>
#include <llvm/ADT/StringRef.h>
#include <cstdint>

namespace clang {
namespace mrdox {

/** A dynamic list of Generator
*/
class MRDOX_VISIBLE
    Generators
{
    Generators(Generators const&) = delete;
    Generators& operator=(Generators const&) = delete;

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
    MRDOX_DECL
    virtual
    ~Generators() = default;

    /** Return an iterator to the beginning.
    */
    MRDOX_DECL
    virtual
    iterator
    begin() const noexcept = 0;

    /** Return an iterator to the end.
    */
    MRDOX_DECL
    virtual
    iterator
    end() const noexcept = 0;

    /** Return a pointer to the matching generator.

        @return A pointer to the generator, or `nullptr`.

        @param name The name of the generator. The
        name must be an exact match, including case.
    */
    MRDOX_DECL
    virtual
    Generator const*
    find(
        llvm::StringRef name) const noexcept = 0;
};

/** Return a reference to the global Generators instance.
*/
MRDOX_DECL
Generators const&
getGenerators() noexcept;

} // mrdox
} // clang

#endif
