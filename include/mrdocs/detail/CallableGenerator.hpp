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

#ifndef MRDOCS_API_DETAIL_CALLABLEGENERATOR_HPP
#define MRDOCS_API_DETAIL_CALLABLEGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace detail {

// A generator whose work is a callable registered under an id.
template <class F>
class CallableGenerator final
    : public Generator
{
    std::string id_;
    std::string displayName_;
    std::string fileExtension_;
    F fn_;

public:
    CallableGenerator(
        std::string_view id,
        std::string_view displayName,
        std::string_view fileExtension,
        F fn)
        : id_(id)
        , displayName_(displayName.empty() ? id : displayName)
        , fileExtension_(fileExtension)
        , fn_(std::move(fn))
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
        return displayName_;
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return fileExtension_;
    }

    Expected<void>
    build(
        Corpus const& corpus,
        Config const& config,
        report::Reporter& reporter) const override
    {
        GeneratorExtensionContext const context =
            makeGeneratorExtensionContext(*this, corpus, config, reporter);
        F fn = fn_;
        return fn(context);
    }
};

} // detail

} // mrdocs

#endif // MRDOCS_API_DETAIL_CALLABLEGENERATOR_HPP
