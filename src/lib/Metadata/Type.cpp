//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

dom::String
toString(
    QualifierKind kind) noexcept
{
    switch(+kind)
    {
    case QualifierKind::None:
        return "";
    case QualifierKind::Const:
        return "const";
    case QualifierKind::Volatile:
        return "volatile";
    case QualifierKind::Const | QualifierKind::Volatile:
        return "const volatile";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(
    TypeKind kind) noexcept
{
    switch(kind)
    {
    case TypeKind::Named:
        return "named";
    case TypeKind::Decltype:
        return "decltype";
    case TypeKind::Auto:
        return "auto";
    case TypeKind::LValueReference:
        return "lvalue-reference";
    case TypeKind::RValueReference:
        return "rvalue-reference";
    case TypeKind::Pointer:
        return "pointer";
    case TypeKind::MemberPointer:
        return "member-pointer";
    case TypeKind::Array:
        return "array";
    case TypeKind::Function:
        return "function";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(
    AutoKind kind) noexcept
{
    switch(kind)
    {
    case AutoKind::Auto:
        return "auto";
    case AutoKind::DecltypeAuto:
        return "decltype(auto)";
    default:
        MRDOCS_UNREACHABLE();
    }
}

SymbolID
TypeInfo::
namedSymbol() const noexcept
{
    if (!isNamed())
    {
        return SymbolID::invalid;
    }
    auto const* NT = dynamic_cast<NamedTypeInfo const*>(this);
    if (!NT->Name)
    {
        return SymbolID::invalid;
    }
    return NT->Name->id;
}

namespace {

constexpr
struct TypeBeforeWriter
{
    template<
        typename T,
        bool NeedParens>
    inline
    void
    operator()(
        T const& t,
        auto& write,
        std::bool_constant<NeedParens>) const;

} writeTypeBefore;

constexpr
struct TypeAfterWriter
{
    template<
        typename T,
        bool NeedParens>
    inline
    void
    operator()(
        T const& t,
        auto& write,
        std::bool_constant<NeedParens>) const;

} writeTypeAfter;

template<typename T>
void
writeFullType(
    T const& t,
    auto& write)
{
    visit(t, writeTypeBefore, write, std::false_type{});
    visit(t, writeTypeAfter, write, std::false_type{});
}

template<
    typename T,
    bool NeedParens>
inline
void
TypeBeforeWriter::
operator()(
    T const& t,
    auto& write,
    std::bool_constant<NeedParens>) const
{
    if (TypeInfo const* inner = innerTypePtr(t))
    {
        visit(*inner, *this, write, std::bool_constant<requires {
            t.PointeeType;
        }>{});
    }

    if(t.IsPackExpansion)
        write("...");

    if constexpr(T::isNamed())
    {
        if (t.IsConst)
        {
            write("const", ' ');
        }
        if (t.IsVolatile)
        {
            write("volatile", ' ');
        }
    }

    if constexpr(requires { t.ParentType; })
    {
        if(t.ParentType)
        {
            writeFullType(*t.ParentType, write);
            write("::");
        }
    }

    if constexpr(T::isDecltype())
        write("decltype(", t.Operand.Written, ')');

    if constexpr(T::isAuto())
    {
        if(t.Constraint)
            write(toString(*t.Constraint), ' ');
        switch(t.Keyword)
        {
        case AutoKind::Auto:
            write("auto");
            break;
        case AutoKind::DecltypeAuto:
            write("decltype(auto)");
            break;
        default:
            MRDOCS_UNREACHABLE();
        }
    }

    if constexpr(T::isNamed())
        write(toString(*t.Name));

    if constexpr(requires { t.PointeeType; })
    {
        switch(T::kind_id)
        {
        case TypeKind::LValueReference:
            write('&');
            break;
        case TypeKind::RValueReference:
            write("&&");
            break;
        case TypeKind::Pointer:
        case TypeKind::MemberPointer:
            write('*');
            break;
        default:
            MRDOCS_UNREACHABLE();
        }

        if (t.IsConst)
        {
            write(' ', "const");
        }
        if (t.IsVolatile)
        {
            write(' ', "volatile");
        }
    }

    if constexpr(NeedParens &&
        (T::isArray() || T::isFunction()))
        write('(');
}

template<
    typename T,
    bool NeedParens>
inline
void
TypeAfterWriter::
operator()(
    T const& t,
    auto& write,
    std::bool_constant<NeedParens>) const
{
    if constexpr(NeedParens &&
        (T::isArray() || T::isFunction()))
        write(')');

    if constexpr(T::isArray())
        write('[', t.Bounds.Value ?
            std::to_string(*t.Bounds.Value) :
            t.Bounds.Written, ']');

    if constexpr(T::isFunction())
    {
        write('(');
        if(! t.ParamTypes.empty())
        {
            writeFullType(*t.ParamTypes.front(), write);
            for(auto first = t.ParamTypes.begin();
                ++first != t.ParamTypes.end();)
            {
                write(", ");
                writeFullType(**first, write);
            }
        }

        if(t.IsVariadic)
        {
            if(! t.ParamTypes.empty())
                write(", ");
            write("...");
        }

        write(')');

        if (t.IsConst)
        {
            write(' ', "const");
        }
        if (t.IsVolatile)
        {
            write(' ', "volatile");
        }

        if (t.RefQualifier != ReferenceKind::None)
        {
            write(' ', toString(t.RefQualifier));
        }

        if (auto spec = toString(t.ExceptionSpec); !spec.empty())
        {
            write(' ', spec);
        }
    }

    if (TypeInfo const* inner = innerTypePtr(t))
    {
        visit(*inner, *this, write, std::bool_constant<requires {
            t.PointeeType;
        }>{});
    }
}

void
writeTypeTo(
    std::string& result,
    auto&&... args)
{
    (result += ... += args);
}

} // (anon)

std::strong_ordering
NamedTypeInfo::
operator<=>(NamedTypeInfo const& other) const
{
    if (auto const br = dynamic_cast<TypeInfo const&>(*this) <=> dynamic_cast<TypeInfo const&>(other);
        !std::is_eq(br))
    {
        return br;
    }
    return Name <=> other.Name;
}

std::strong_ordering
AutoTypeInfo::
operator<=>(AutoTypeInfo const&) const = default;

std::strong_ordering
LValueReferenceTypeInfo::
operator<=>(LValueReferenceTypeInfo const&) const = default;

std::strong_ordering
RValueReferenceTypeInfo::
operator<=>(RValueReferenceTypeInfo const&) const = default;

std::strong_ordering
PointerTypeInfo::
operator<=>(PointerTypeInfo const&) const = default;

std::strong_ordering
MemberPointerTypeInfo::
operator<=>(MemberPointerTypeInfo const&) const = default;

std::strong_ordering
ArrayTypeInfo::
operator<=>(ArrayTypeInfo const&) const = default;

std::strong_ordering
FunctionTypeInfo::
operator<=>(FunctionTypeInfo const& other) const {
    if (auto const r = dynamic_cast<TypeInfo const&>(*this) <=>
             dynamic_cast<TypeInfo const&>(other);
        !std::is_eq(r))
    {
        return r;
    }
    if (auto const r = ReturnType <=> other.ReturnType;
        !std::is_eq(r))
    {
        return r;
    }
    if (auto const r = ParamTypes.size() <=> other.ParamTypes.size();
        !std::is_eq(r))
    {
        return r;
    }
    for (std::size_t i = 0; i < ParamTypes.size(); ++i)
    {
        if (auto const r = ParamTypes[i] <=> other.ParamTypes[i];
            !std::is_eq(r))
        {
            return r;
        }
    }
    return std::tie(IsConst, IsVolatile, RefQualifier, ExceptionSpec, IsVariadic) <=>
           std::tie(other.IsConst, IsVolatile, other.RefQualifier, other.ExceptionSpec, other.IsVariadic);
}


std::string
toString(
    TypeInfo const& T,
    std::string_view Name)
{
    auto write = [result = std::string()](
        auto&&... args) mutable
        {
            if constexpr(sizeof...(args))
                writeTypeTo(result, args...);
            else
                return result;
        };
    visit(T, writeTypeBefore, write, std::false_type{});
    if(! Name.empty())
        write(' ', Name);
    visit(T, writeTypeAfter, write, std::false_type{});
    return write();
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    TypeInfo const& I,
    DomCorpus const* domCorpus)
{
    io.map("class", std::string("type"));
    io.map("kind", I.Kind);
    io.map("is-pack", I.IsPackExpansion);
    visit(I, [&io, domCorpus]<typename T>(T const& t)
    {
        if constexpr(T::isNamed())
        {
            io.map("name", t.Name);
        }
        if constexpr(T::isDecltype())
        {
            io.map("operand", t.Operand.Written);
        }
        if constexpr(T::isAuto())
        {
            io.map("keyword", t.Keyword);
            if (t.Constraint)
            {
                io.map("constraint", t.Constraint);
            }
        }
        io.map("is-const", t.IsConst);
        io.map("is-volatile", t.IsVolatile);
        if constexpr(requires { t.ParentType; })
        {
            io.map("parent-type", t.ParentType);
        }
        if constexpr(requires { t.PointeeType; })
        {
            io.map("pointee-type", t.PointeeType);
        }
        if constexpr(T::isArray())
        {
            io.map("element-type", t.ElementType);
            if(t.Bounds.Value)
            {
                io.map("bounds-value", *t.Bounds.Value);
            }
            io.map("bounds-expr", t.Bounds.Written);
        }
        if constexpr(T::isFunction())
        {
            io.map("return-type", t.ReturnType);
            io.map("param-types", dom::LazyArray(t.ParamTypes, domCorpus));
            io.map("exception-spec", t.ExceptionSpec);
            io.map("ref-qualifier", t.RefQualifier);
            io.map("is-variadic", t.IsVariadic);
        }
    });
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypeInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

std::strong_ordering
operator<=>(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs)
{
    if (lhs && rhs)
    {
        auto& lhsRef = *lhs;
        auto& rhsRef = *rhs;
        if (lhsRef.Kind == rhsRef.Kind)
        {
            return visit(lhsRef, detail::VisitCompareFn<TypeInfo>(rhsRef));
        }
        return lhsRef.Kind <=> rhsRef.Kind;
    }
    return !lhs ? std::strong_ordering::less
            : std::strong_ordering::greater;
}

namespace {
template <
    class TypeInfoTy,
    bool isMutable = !std::is_const_v<std::remove_reference_t<TypeInfoTy>>,
    class Ptr = std::conditional_t<isMutable, Polymorphic<TypeInfo>*, Polymorphic<TypeInfo> const*>,
    class Ref = std::conditional_t<isMutable, std::reference_wrapper<Polymorphic<TypeInfo>>, std::reference_wrapper<Polymorphic<TypeInfo> const>>>
requires std::same_as<std::remove_cvref_t<TypeInfoTy>, TypeInfo>
std::optional<Ref>
innerTypeImpl(TypeInfoTy&& TI) noexcept
{
    // Get a pointer to the inner type
    Ptr innerPtr = visit(TI, []<typename T>(T& t) -> Ptr
    {
        if constexpr(requires { t.PointeeType; })
        {
            return &t.PointeeType;
        }
        if constexpr(requires { t.ElementType; })
        {
            return &t.ElementType;
        }
        if constexpr(requires { t.ReturnType; })
        {
            return &t.ReturnType;
        }
        return nullptr;
    });
    // Convert pointer to reference wrapper if possible
    if (innerPtr)
    {
        if constexpr (isMutable)
        {
            return std::ref(*innerPtr);
        }
        else
        {
            return std::cref(*innerPtr);
        }
    }
    return std::nullopt;
}

template <class TypeInfoTy>
auto
innerTypePtrImpl(TypeInfoTy&& TI) noexcept
{
    auto res = innerTypeImpl(TI);
    if (res)
    {
        auto& ref = res->get();
        return &*ref;
    }
    return decltype(&*res->get())(nullptr);
}

template <class TypeInfoTy>
auto
innermostTypeImpl(TypeInfoTy&& TI) noexcept
{
    auto inner = innerTypeImpl(TI);
    while (inner)
    {
        auto& ref = inner->get();
        if (!ref)
        {
            return inner;
        }
        if (ref->isNamed())
        {
            return inner;
        }
        inner = innerTypeImpl(*ref);
    }
    return inner;
}

}

std::optional<std::reference_wrapper<Polymorphic<TypeInfo> const>>
innerType(TypeInfo const& TI) noexcept
{
    return innerTypeImpl(TI);
}

std::optional<std::reference_wrapper<Polymorphic<TypeInfo>>>
innerType(TypeInfo& TI) noexcept
{
    return innerTypeImpl(TI);
}

TypeInfo const*
innerTypePtr(TypeInfo const& TI) noexcept
{
    return innerTypePtrImpl(TI);
}

TypeInfo*
innerTypePtr(TypeInfo& TI) noexcept
{
    return innerTypePtrImpl(TI);
}

std::optional<std::reference_wrapper<Polymorphic<TypeInfo> const>>
innermostType(TypeInfo const& TI) noexcept
{
    return innermostTypeImpl(TI);
}

std::optional<std::reference_wrapper<Polymorphic<TypeInfo>>>
innermostType(TypeInfo& TI) noexcept
{
    return innermostTypeImpl(TI);
}

} // clang::mrdocs
