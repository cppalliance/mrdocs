//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TPARAM_HPP
#define MRDOCS_API_METADATA_TPARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TParam/ConstantTParam.hpp>
#include <mrdocs/Metadata/TParam/TParamBase.hpp>
#include <mrdocs/Metadata/TParam/TemplateTParam.hpp>
#include <mrdocs/Metadata/TParam/TypeTParam.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

// Register TParam's concrete kinds for the generic visit
// (Support/Reflection/Describe.hpp).
#define INFO(X) MRDOCS_KIND_ENTRY(TParam, X##TParam)
MRDOCS_DESCRIBE_KINDS_BEGIN(TParam)
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(TParam)
#undef INFO

/** Compare polymorphic template parameters.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs);

/** Equality helper for polymorphic template parameters.
*/
inline
bool
operator==(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}


} // mrdocs

#endif
