//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Console.hpp"

#include <mrdocs/Dom.hpp>

#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <string>

namespace mrdocs::js {
namespace {

// Render one argument the way console.log would print it. Strings
// pass through verbatim; objects and arrays go through JSON
// serialisation so a script doesn't end up reading `[object Object]`;
// everything else falls back to the DOM's own `toString`.
std::string
formatArg(dom::Value const& v)
{
    if (v.isString())
    {
        return std::string(v.getString());
    }
    if (v.isObject() || v.isArray())
    {
        return dom::JSON::stringify(v);
    }
    return toString(v);
}

void
writeLine(llvm::raw_ostream& os, dom::Array const& args)
{
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            os << ' ';
        }
        os << formatArg(args.get(i));
    }
    os << '\n';
    os.flush();
}

dom::Object
buildConsole()
{
    dom::Object console;
    console.set("log", dom::Value(dom::makeVariadicInvocable(
        [](dom::Array const& args) -> Expected<dom::Value, Error>
        {
            writeLine(llvm::outs(), args);
            return dom::Value();
        })));
    console.set("error", dom::Value(dom::makeVariadicInvocable(
        [](dom::Array const& args) -> Expected<dom::Value, Error>
        {
            writeLine(llvm::errs(), args);
            return dom::Value();
        })));
    return console;
}

} // (anon)

void
registerConsole(Scope& scope)
{
    scope.setGlobal("console", dom::Value(buildConsole()));
}

} // namespace mrdocs::js
