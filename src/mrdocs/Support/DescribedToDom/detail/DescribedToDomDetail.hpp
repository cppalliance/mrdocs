//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_DETAIL_DETAIL_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_DETAIL_DETAIL_HPP

#include <mrdocs/Support/DescribedToDom/DescribedToDomForward.hpp>

// Private helpers shared by the DescribedToDom proxies (object and array).
namespace mrdocs {

namespace dom::detail {

/** Map a described member's C++ name to its DOM field key.

    DOM field keys are camelCase, so a member's PascalCase C++ name is
    lowered in its first character only: `Name` becomes `name`, `IsConst`
    becomes `isConst`. This is the whole transform: the rest of the name
    already matches, because member names are single camelCase words with
    no separators. It is distinct from the kebab-case used for enum and
    kind *values* (e.g. `named-type`); member *keys* are camelCase.

    Used by @ref DescribedObjectProxy to key its fields (see
    DescribedToDomObject.hpp).

    @param name The member's C++ name.
    @return The DOM field key.
*/
inline
std::string
normalizeMemberName(std::string_view name)
{
    std::string out(name);
    if (!out.empty() && out[0] >= 'A' && out[0] <= 'Z')
    {
        out[0] = static_cast<char>(out[0] - 'A' + 'a');
    }
    return out;
}

/** Collect the unqualified names of all base classes of `T`, recursively.

    Appends each base class name (via @ref unqualifiedTypeName)
    to `out`, depth-first, so a template can test inheritance with
    `contains $meta.bases "Symbol"`. The type name comes from the one
    shared reflection utility, not a proxy-local copy.

    @tparam T The described type whose base names to collect.
    @param out The array to append base names to.
*/
template <typename T>
void
proxyCollectBaseNames(mrdocs::dom::Array& out)
{
    if constexpr (mrdocs::describe::has_describe_bases<T>::value)
    {
        mrdocs::describe::for_each(
            mrdocs::describe::describe_bases<T>{},
            [&](auto const& descriptor)
            {
                using BaseType =
                    typename std::decay_t<decltype(descriptor)>::type;
                out.push_back(std::string(
                    mrdocs::unqualifiedTypeName<BaseType>()));
                proxyCollectBaseNames<BaseType>(out);
            });
    }
}

/** Whether a member value should appear in the reflection DOM.

    Mirrors the XML writer's omission rule (`isOmittedFromXML`) so the
    reflection view matches "do the same as XML": a member whose value is
    "empty" is treated as absent and skipped by `get`/`exists`/`visit`/
    `size`. The rule is per value category; everything not listed is
    always emitted.

    @param value The member value to test.
    @return `true` when the value should be emitted, `false` to omit it.

    Examples:
    @li `Optional` -> emitted only when engaged: `Optional<T>{}` -> `false`,
        `Optional<T>{x}` -> `true`.
    @li `std::string` -> emitted when non-empty: `""` -> `false`,
        `"x"` -> `true`.
    @li a described enum with an undefined state (e.g. `AccessKind`) ->
        emitted unless it holds that state: `AccessKind::None` -> `false`,
        `AccessKind::Public` -> `true`.
    @li an `ExprInfo` -> emitted when it has a written form:
        `{.Written = ""}` -> `false`, `{.Written = "n > 0"}` -> `true`.
    @li anything else (an `int`, a `bool`, a nested described object) ->
        always `true`.
*/
template <class T>
bool
proxyShouldEmit(T const& value)
{
    if constexpr (mrdocs::specialization_of<T, mrdocs::Optional>)
    {
        return value.has_value();
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return !value.empty();
    }
    else if constexpr (mrdocs::describe::has_undefined_enumerator<T>)
    {
        return value != mrdocs::describe::undefined_enumerator<T>;
    }
    else if constexpr (mrdocs::describe::has_describe_members<T>::value)
    {
        // A described struct is present unless every one of its members
        // is itself omitted (recursively) -- the same rule the XML
        // writer applies (isOmittedFromXML). This covers ExprInfo (its
        // one string member), ConstantExprInfo (that string plus an
        // optional value), and any other struct, without naming a type.
        bool anyEmitted = false;
        mrdocs::describe::for_each_member<T>(
            [&](auto d)
            {
                if (proxyShouldEmit(value.*d.pointer))
                {
                    anyEmitted = true;
                }
            });
        return anyEmitted;
    }
    else
    {
        return true;
    }
}

/** The `$meta` object exposed for every proxied type `T`.

    Replaces the `addMetaObject` the old io.map path synthesized. It
    carries the reflected type identity a template routes on:

    @li `type`: the unqualified C++ type name, e.g. `"FunctionSymbol"`.
    @li `bases`: the recursive base class names, e.g. `["Symbol"]`,
        letting a template test `contains $meta.bases "Symbol"`.

    Both are compile-time constant for a given `T`, so the object is
    built once per instantiation and cached; each call returns the same
    shared handle instead of allocating a fresh object.

    @tparam T The described type whose `$meta` to build.
    @return The shared `$meta` object for `T`.
*/
template <typename T>
mrdocs::dom::Object
proxyMetaObject()
{
    static mrdocs::dom::Object const meta = []
    {
        mrdocs::dom::Object m;
        m.set("type", std::string(mrdocs::unqualifiedTypeName<T>()));
        mrdocs::dom::Array bases;
        proxyCollectBaseNames<T>(bases);
        m.set("bases", std::move(bases));
        return m;
    }();
    return meta;
}

} // namespace dom::detail

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_DETAIL_DETAIL_HPP
