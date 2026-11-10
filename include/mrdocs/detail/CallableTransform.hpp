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

#ifndef MRDOCS_API_DETAIL_CALLABLETRANSFORM_HPP
#define MRDOCS_API_DETAIL_CALLABLETRANSFORM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Transform.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace detail {

// A transform whose work is a callable registered under an id.
template <class F>
class CallableTransform final
    : public Transform
{
    std::string id_;
    F fn_;

public:
    CallableTransform(std::string_view id, F fn)
        : id_(id)
        , fn_(std::move(fn))
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    Expected<void>
    apply(
        Corpus& corpus,
        Config const& config,
        report::Reporter& reporter) const override
    {
        TransformExtensionContext const context =
            makeTransformExtensionContext(*this, corpus, config, reporter);
        // The callable is called through a copy: `apply` is const and the
        // callable may be a mutable lambda.
        F fn = fn_;
        return fn(context);
    }
};

} // detail

} // mrdocs

#endif // MRDOCS_API_DETAIL_CALLABLETRANSFORM_HPP
