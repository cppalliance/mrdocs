//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMSYMBOL_HPP
#define MRDOX_API_DOM_DOMSYMBOL_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>
#include <type_traits>

namespace clang {
namespace mrdox {

/** Any Info-derived type.
*/
template<class T>
class MRDOX_DECL
    DomSymbol : public dom::Object
{
    static_assert(! std::is_same_v<T, Info>);
    static_assert(std::derived_from<T, Info>);

protected:
    T const& I_;
    Corpus const& corpus_;

public:
    DomSymbol(T const& I, Corpus const& corpus) noexcept;
    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

// Explicit instantiation externs
extern template class MRDOX_DECL DomSymbol<NamespaceInfo>;
extern template class MRDOX_DECL DomSymbol<RecordInfo>;
extern template class MRDOX_DECL DomSymbol<FunctionInfo>;
extern template class MRDOX_DECL DomSymbol<EnumInfo>;
extern template class MRDOX_DECL DomSymbol<TypedefInfo>;
extern template class MRDOX_DECL DomSymbol<VariableInfo>;
extern template class MRDOX_DECL DomSymbol<FieldInfo>;
extern template class MRDOX_DECL DomSymbol<SpecializationInfo>;

} // mrdox
} // clang

#endif
