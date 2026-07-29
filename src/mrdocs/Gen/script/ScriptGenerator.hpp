//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_SCRIPT_SCRIPTGENERATOR_HPP
#define MRDOCS_LIB_GEN_SCRIPT_SCRIPTGENERATOR_HPP

#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs::script {

/** A Generator backed by a script-defined `dom::Function`.

    A generator declared with `mrdocs.register_generator(id, fn)` is a
    `dom::Function` that owns its whole emit. This wraps one such function
    as a @ref Generator so the host treats it like any other: it is
    collected into an @ref ExtensionRegistry, looked up by id, and run
    through @ref Generator::build, which invokes the function with the
    generator `ctx` object.

    The function is language-agnostic: a `dom::Function` self-owns its
    scripting VM, so one implementation drives a Lua or a JavaScript
    generator without the host knowing which. The generator also carries a
    strong reference to its engine (`vm`, a type-erased Context handle), so
    it stays runnable on its own without any external owner keeping the
    scripting VM alive.
*/
class ScriptGenerator
    : public Generator
{
    std::string id_;
    dom::Function fn_;
    std::shared_ptr<void> vm_;

public:
    ScriptGenerator(std::string id, dom::Function fn, std::shared_ptr<void> vm)
        : id_(std::move(id))
        , fn_(std::move(fn))
        , vm_(std::move(vm))
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    std::string_view
    displayName() const noexcept override
    {
        return id_;
    }

    // A script generator owns its whole emit, so it has no single output
    // extension the host resolves for it.
    std::string_view
    fileExtension() const noexcept override
    {
        return {};
    }

    Expected<void>
    build(Corpus const& corpus, Config const& config) const override;
};

} // mrdocs::script

#endif
