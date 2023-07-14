//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_ARRAY_IPP
#define MRDOX_API_DOM_ARRAY_IPP

namespace clang {
namespace mrdox {
namespace dom {

//------------------------------------------------
//
// Array
//
//------------------------------------------------

inline char const* Array::type_key() const noexcept
{
    return impl_->type_key();
}

inline bool Array::empty() const noexcept
{
    return impl_->size() == 0;
}

inline auto Array::size() const noexcept -> size_type
{
    return impl_->size();
}

inline auto Array::get(std::size_t i) const -> value_type
{
    return impl_->get(i);
}

inline auto Array::operator[](std::size_t i) const -> value_type
{
    return get(i);
}

inline auto Array::at(std::size_t i) const -> value_type
{
    if(i < size())
        return get(i);
    Error("out of range").Throw();
}

inline void Array::emplace_back(value_type value)
{
    impl_->emplace_back(std::move(value));
}

} // dom
} // mrdox
} // clang

#endif
