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

#ifndef MRDOX_FUNCTIONS_HPP
#define MRDOX_FUNCTIONS_HPP

#include "Types.h"
#include <clang/Basic/Specifiers.h>
#include <vector>

namespace clang {
namespace mrdox {

//------------------------------------------------

template<class ValueType>
class List
{
    using list_type = std::vector<ValueType>;

public:
    using value_type = typename list_type::value_type;
    using reference = typename list_type::reference;
    using iterator = typename list_type::iterator;
    using const_reference = typename list_type::const_reference;
    using const_iterator = typename list_type::const_iterator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    bool empty() const noexcept { return v_.empty(); }
    size_type size() const noexcept { return v_.size(); }
    iterator begin() noexcept { return v_.begin(); }
    iterator end() noexcept { return v_.end(); }
    const_iterator begin() const noexcept { return v_.begin(); }
    const_iterator end() const noexcept { return v_.end(); }
    reference front() { return v_.front(); }
    reference back() { return v_.back(); }
    const_reference front() const { return v_.front(); }
    const_reference back() const { return v_.back(); }
    reference operator[](std::size_t i) { return v_[i]; }
    const_reference operator[](std::size_t i) const { return v_[i]; }

protected:
    List() = default;
    List(List&&) noexcept = default;
    List& operator=(
        List&&) noexcept = default;

    list_type v_;
};

//------------------------------------------------

/** A list of overloads for a function.
*/
struct FunctionOverloads
    : List<FunctionInfo>
{
    /// The name of the function.
    UnqualifiedName name;

    void insert(FunctionInfo I);
    void merge(FunctionOverloads&& other);

    FunctionOverloads(
        FunctionOverloads&&) noexcept = default;
    FunctionOverloads& operator=(
        FunctionOverloads&&) noexcept = default;
    FunctionOverloads(
        FunctionInfo I);
};

//------------------------------------------------

/** A list of functions, each with possible overloads.
*/
struct FunctionList
    : List<FunctionOverloads>
{
    clang::AccessSpecifier access;

    void insert(FunctionInfo I);
    void merge(FunctionList&& other);

    FunctionList(
        FunctionList&&) noexcept = default;
    FunctionList(
        clang::AccessSpecifier access_ =
            clang::AccessSpecifier::AS_public) noexcept
        : access(access_)
    {
    }

private:
    iterator find(llvm::StringRef name) noexcept;
};

//------------------------------------------------

} // mrdox
} // clang

#endif
