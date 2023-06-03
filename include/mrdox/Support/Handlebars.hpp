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

#ifndef MRDOX_SUPPORT_HANDLEBARS_HPP
#define MRDOX_SUPPORT_HANDLEBARS_HPP

#include <mrdox/Platform.hpp>
#include <cstddef>
#include <string_view>
#include <system_error>

namespace clang {
namespace mrdox {
namespace hbs {

struct Access;
class Object;

//------------------------------------------------

/** A reference to an instance of the Javascript interpreter.
*/
class Context
{
    struct Impl;

    Impl* impl_;

    friend struct Access;

public:
    Context& operator=(Context const&) = delete;

    MRDOX_DECL ~Context();
    MRDOX_DECL Context();
    MRDOX_DECL Context(Context const&) noexcept;

    MRDOX_DECL std::error_code eval(std::string_view js);
    MRDOX_DECL std::error_code eval_file(std::string_view path);
};

//------------------------------------------------

/** An ECMAScript Array.
*/
class Array
{
    Context ctx_;
    int idx_;

    friend struct Access;

public:
    Array(Array const&) = delete;
    Array& operator=(Array const&) = delete;

    MRDOX_DECL ~Array();
    MRDOX_DECL Array(Context const& ctx);
    MRDOX_DECL void append(std::string_view value) const;
    MRDOX_DECL void append(Array const& value) const;
    MRDOX_DECL void append(Object const& value) const;
};

//------------------------------------------------

/** An ECMAScript Object
*/
class Object
{
    Context ctx_;
    int idx_;

    friend struct Access;

public:
    Object(Object const&) = delete;
    Object& operator=(Object const&) = delete;

    MRDOX_DECL ~Object();
    MRDOX_DECL Object(Context const& ctx);
    MRDOX_DECL void insert(std::string_view key, std::string_view value) const;
    MRDOX_DECL void insert(std::string_view key, Array const& value) const;
    MRDOX_DECL void insert(std::string_view key, Object const& value) const;
};

//------------------------------------------------

/** A compiled Handlebars template.
*/
class Template
{
public:
};

/** A compiled Handlebars partial.
*/
class Partial
{
public:
};

//------------------------------------------------

/** An instance of the handlebars template engine.
*/
class Handlebars
{
    Context ctx_;

public:
    explicit
    Handlebars(Context const& ctx) noexcept;

    /** Return the loaded handlebars script.
    */
    friend
    Handlebars
    loadHandlebarsScript(
        std::string_view path,
        std::error_code& ec);
};

} // hbs
} // mrdox
} // clang

#endif
