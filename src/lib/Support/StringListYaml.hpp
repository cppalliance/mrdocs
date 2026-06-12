//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_STRINGLISTYAML_HPP
#define MRDOCS_LIB_SUPPORT_STRINGLISTYAML_HPP

#include <mrdocs/Support/StringList.hpp>
#include <llvm/Support/YAMLTraits.h>
#include <string>
#include <vector>

namespace mrdocs {

/** Placeholder returned for the (invalid) mapping form of a StringList.

    `StringList` accepts a scalar or a sequence, never a mapping. The
    polymorphic YAML dispatch still has to compile a mapping branch, so
    that branch yamlizes this type, whose traits simply report an error.
*/
struct StringListMapError {};

} // mrdocs

template <>
struct llvm::yaml::MappingTraits<mrdocs::StringListMapError>
{
    static void
    mapping(llvm::yaml::IO& io, mrdocs::StringListMapError&)
    {
        io.setError("expected a string or a list of strings");
    }
};

template <>
struct llvm::yaml::PolymorphicTraits<mrdocs::StringList>
{
    static NodeKind
    getKind(mrdocs::StringList const& v)
    {
        // On output, emit a single scalar when there is exactly one
        // value and a sequence otherwise.
        return v.values.size() == 1
            ? NodeKind::Scalar
            : NodeKind::Sequence;
    }

    static std::string&
    getAsScalar(mrdocs::StringList& v)
    {
        v.values.clear();
        v.values.emplace_back();
        return v.values.front();
    }

    static std::vector<std::string>&
    getAsSequence(mrdocs::StringList& v)
    {
        v.values.clear();
        return v.values;
    }

    static mrdocs::StringListMapError&
    getAsMap(mrdocs::StringList&)
    {
        static thread_local mrdocs::StringListMapError dummy;
        return dummy;
    }
};

#endif // MRDOCS_LIB_SUPPORT_STRINGLISTYAML_HPP
