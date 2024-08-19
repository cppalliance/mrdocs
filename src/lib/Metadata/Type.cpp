//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang {
namespace mrdocs {

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
    if(! isNamed())
        return SymbolID::invalid;
    const auto* NT = static_cast<
        const NamedTypeInfo*>(this);
    if(! NT->Name)
        return SymbolID::invalid;
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
        const T& t,
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
        const T& t,
        auto& write,
        std::bool_constant<NeedParens>) const;

} writeTypeAfter;

template<typename T>
void
writeFullType(
    const T& t,
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
    const T& t,
    auto& write,
    std::bool_constant<NeedParens>) const
{
    if(TypeInfo* inner = t.innerType())
        visit(*inner, *this, write, std::bool_constant<
            requires { t.PointeeType; }>{});

    if(t.IsPackExpansion)
        write("...");

    if constexpr(T::isNamed())
    {
        if(t.CVQualifiers != QualifierKind::None)
            write(toString(t.CVQualifiers), ' ');
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

        if constexpr(requires { t.CVQualifiers; })
        {
            if(t.CVQualifiers != QualifierKind::None)
                write(' ', toString(t.CVQualifiers));
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
    const T& t,
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

        if(t.CVQualifiers != QualifierKind::None)
            write(' ', toString(t.CVQualifiers));

        if(t.RefQualifier != ReferenceKind::None)
            write(' ', toString(t.RefQualifier));

        if(auto spec = toString(t.ExceptionSpec); ! spec.empty())
            write(' ', spec);
    }

    if(TypeInfo* inner = t.innerType())
        visit(*inner, *this, write, std::bool_constant<
            requires { t.PointeeType; }>{});
}

void
writeTypeTo(
    std::string& result,
    auto&&... args)
{
    (result += ... += args);
}

} // (anon)

std::string
toString(
    const TypeInfo& T,
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

} // mrdocs
} // clang
