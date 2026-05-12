//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// Kind descriptors for polymorphic bases.
//
// Extends the reflection machinery in Describe.hpp with the
// `MRDOCS_DESCRIBE_KINDS` macro, which registers the closed set of
// concrete derived classes ("kinds") of a polymorphic base. Generic
// code can then iterate the resulting list with `describe::for_each`
// and dispatch over kinds without per-base X-macro boilerplate.
//
// This support lives in its own header so consumers that only need
// `MRDOCS_DESCRIBE_STRUCT`, `MRDOCS_DESCRIBE_CLASS`, and
// `MRDOCS_DESCRIBE_ENUM` keep a slimmer include.

#ifndef MRDOCS_SUPPORT_DESCRIBEKINDS_HPP
#define MRDOCS_SUPPORT_DESCRIBEKINDS_HPP

#include <mrdocs/Support/Describe.hpp>
#include <type_traits>

namespace mrdocs::describe {

// -------------------------------------------------------------------
// Kind descriptors
// -------------------------------------------------------------------

/** Descriptor for one concrete kind of a polymorphic base.

    Carries the static assertion that `D` actually derives from `C`
    and exposes the derived type as the nested `type` alias.

    @tparam C The polymorphic base class.
    @tparam D A concrete derived class registered with
        @ref MRDOCS_DESCRIBE_KINDS.
*/
template<class C, class D>
struct kind_descriptor
{
    static_assert(std::is_base_of_v<C, D>,
        "A type listed as a kind is not actually derived from C");
    using type = D;
};

template<class... T>
list<T...> kind_descriptor_fn_impl(int, T...);

// -------------------------------------------------------------------
// Query alias (ADL lookup)
// -------------------------------------------------------------------

/** Compile-time list of the kinds registered for a polymorphic base.

    Resolves via ADL to the @ref list produced by
    @ref MRDOCS_DESCRIBE_KINDS. Use with @ref for_each to dispatch
    over every concrete kind of `T`.

    @tparam T The polymorphic base class.
*/
template<class T>
using describe_kinds =
    decltype(mrdocs_kind_descriptor_fn(static_cast<T**>(nullptr)));

// -------------------------------------------------------------------
// Type trait
// -------------------------------------------------------------------

namespace detail {

template<class, class = void>
struct has_describe_kinds_impl : std::false_type {};

template<class T>
struct has_describe_kinds_impl<T,
    std::void_t<describe_kinds<T>>> : std::true_type {};

} // namespace detail

/** Trait: whether a polymorphic base has had its kinds registered.

    Evaluates to `std::true_type` when @ref MRDOCS_DESCRIBE_KINDS has
    been applied to `T` and `std::false_type` otherwise. Generic code
    typically guards `describe::for_each` over @ref describe_kinds
    with this trait so it stays well-formed for types that have not
    opted in.

    @tparam T A class type that may or may not have its kinds
        registered.
*/
template<class T>
using has_describe_kinds = detail::has_describe_kinds_impl<T>;

} // namespace mrdocs::describe

// ===================================================================
// MRDOCS_DESCRIBE_KINDS
// ===================================================================

/** Register a polymorphic base together with the closed set of its
    concrete derived classes ("kinds").

    The macro emits a single descriptor that generic code can iterate
    with `describe::for_each`:

    @code
    MRDOCS_DESCRIBE_KINDS(
        Type,
        NamedType, PointerType, ArrayType // ...
    )

    describe::for_each(
        describe::describe_kinds<Type>{},
        [](auto desc) {
            using D = typename decltype(desc)::type;
            // ... do something with D ...
        });
    @endcode

    Constraints:

      - Every listed `D` must inherit (directly or transitively) from
        `C` and must be a complete type at the point of macro
        expansion. The natural home for the macro is therefore a
        dedicated header that includes every derived class's header
        and issues the macro once.
      - The list describes a closed flat set of leaf classes, not a
        tree. Intermediate bases in a deeper inheritance graph are
        not represented. The descriptor lets generic code visit
        every concrete kind of `C`, not every level of inheritance
        under `C`.

    Query types:

      - `describe::has_describe_kinds<C>` tests whether a base has
        been registered.
      - `describe::describe_kinds<C>` is the resulting
        `list<kind_descriptor<C, D>...>`.

    @param C The polymorphic base class.
    @param ... The concrete derived classes of `C`.
*/
// The emitted `mrdocs_kind_descriptor_fn` is only ever read through
// `decltype`; no caller invokes it. Giving it an inline `{ return {}; }`
// body (instead of leaving it as a pure declaration) silences GCC's
// `-Wunused-function`, which fires on internal-linkage declarations
// that are never defined when the macro is invoked in an anonymous
// namespace (the convention in tests with locally-scoped fixtures).
// `inline` keeps the definition ODR-safe across translation units when
// the macro appears in a header. Clang and MSVC stay silent either
// way.
#define MRDOCS_DESCRIBE_KINDS(C, ...)                               \
    static_assert(std::is_class_v<C>,                               \
        "MRDOCS_DESCRIBE_KINDS should only be used with "           \
        "class types");                                             \
    [[maybe_unused]]                                                \
    inline decltype(                                                \
        ::mrdocs::describe::kind_descriptor_fn_impl(0               \
            __VA_OPT__(MRDOCS_PP_FOR_EACH(                          \
                MRDOCS_KIND_ENTRY, C, __VA_ARGS__))                 \
        )) mrdocs_kind_descriptor_fn(C**) { return {}; }

/** Append one kind to a @ref MRDOCS_DESCRIBE_KINDS-style list.

    Used internally by @ref MRDOCS_DESCRIBE_KINDS and exposed so
    that the BEGIN/END variant can drive the list from an
    `.inc` file.

    @param C The polymorphic base class.
    @param D A concrete derived class of `C`.
*/
#define MRDOCS_KIND_ENTRY(C, D)                                     \
    , ::mrdocs::describe::kind_descriptor<C, D>{}

/** Open a @ref MRDOCS_DESCRIBE_KINDS list driven by an `.inc` file.

    Same effect as @ref MRDOCS_DESCRIBE_KINDS, but split so the kind
    list can be sourced from an existing X-macro `.inc` file (the
    typical pattern in MrDocs). Usage:

    @code
    #define INFO(Name) MRDOCS_KIND_ENTRY(Base, Name##Suffix)
    MRDOCS_DESCRIBE_KINDS_BEGIN(Base)
    #include <path/to/Nodes.inc>
    MRDOCS_DESCRIBE_KINDS_END(Base)
    #undef INFO
    @endcode

    @param C The polymorphic base class.
*/
#define MRDOCS_DESCRIBE_KINDS_BEGIN(C)                              \
    static_assert(std::is_class_v<C>,                               \
        "MRDOCS_DESCRIBE_KINDS_BEGIN should only be used "          \
        "with class types");                                        \
    [[maybe_unused]]                                                \
    inline decltype(                                                \
        ::mrdocs::describe::kind_descriptor_fn_impl(0

/** Close a @ref MRDOCS_DESCRIBE_KINDS_BEGIN-opened list.

    @param C The polymorphic base class (must match the argument
        passed to @ref MRDOCS_DESCRIBE_KINDS_BEGIN).
*/
#define MRDOCS_DESCRIBE_KINDS_END(C)                                \
        )) mrdocs_kind_descriptor_fn(C**) { return {}; }

#endif // MRDOCS_SUPPORT_DESCRIBEKINDS_HPP
