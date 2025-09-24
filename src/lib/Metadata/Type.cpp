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
#include <mrdocs/Metadata/Type/QualifierKind.hpp>

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
    if (NT->Name.valueless_after_move())
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
            writeFullType(**t.ParentType, write);
            write("::");
        }
    }

    if constexpr(T::isDecltype())
        write("decltype(", t.Operand.Written, ')');

    if constexpr(T::isAuto())
    {
        if (t.Constraint)
        {
            write(toString(**t.Constraint), ' ');
        }
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
    bool const LhsHoldsFundamentalType = static_cast<bool>(FundamentalType);
    bool const RhsHoldsFundamentalType = static_cast<bool>(other.FundamentalType);
    if (auto const r = LhsHoldsFundamentalType <=> RhsHoldsFundamentalType;
        !std::is_eq(r))
    {
        return r;
    }
    if (FundamentalType && other.FundamentalType)
    {
        return FundamentalType <=> other.FundamentalType;
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

std::string_view
toString(FundamentalTypeKind const kind) noexcept
{
    switch (kind)
    {
    case FundamentalTypeKind::Void:
        return "void";
    case FundamentalTypeKind::Nullptr:
        return "std::nullptr_t";
    case FundamentalTypeKind::Bool:
        return "bool";
    case FundamentalTypeKind::Char:
        return "char";
    case FundamentalTypeKind::SignedChar:
        return "signed char";
    case FundamentalTypeKind::UnsignedChar:
        return "unsigned char";
    case FundamentalTypeKind::Char8:
        return "char8_t";
    case FundamentalTypeKind::Char16:
        return "char16_t";
    case FundamentalTypeKind::Char32:
        return "char32_t";
    case FundamentalTypeKind::WChar:
        return "wchar_t";
    case FundamentalTypeKind::Short:
        return "short";
    case FundamentalTypeKind::UnsignedShort:
        return "unsigned short";
    case FundamentalTypeKind::Int:
        return "int";
    case FundamentalTypeKind::UnsignedInt:
        return "unsigned int";
    case FundamentalTypeKind::Long:
        return "long";
    case FundamentalTypeKind::UnsignedLong:
        return "unsigned long";
    case FundamentalTypeKind::LongLong:
        return "long long";
    case FundamentalTypeKind::UnsignedLongLong:
        return "unsigned long long";
    case FundamentalTypeKind::Float:
        return "float";
    case FundamentalTypeKind::Double:
        return "double";
    case FundamentalTypeKind::LongDouble:
        return "long double";
    default:
        MRDOCS_UNREACHABLE();
    }
}

bool
fromString(std::string_view str, FundamentalTypeKind& kind) noexcept
{
    static constexpr std::pair<std::string_view, FundamentalTypeKind> map[] = {
        {"void", FundamentalTypeKind::Void},
        {"std::nullptr_t", FundamentalTypeKind::Nullptr},
        {"bool", FundamentalTypeKind::Bool},
        {"char", FundamentalTypeKind::Char},
        {"signed char", FundamentalTypeKind::SignedChar},
        {"unsigned char", FundamentalTypeKind::UnsignedChar},
        {"char8_t", FundamentalTypeKind::Char8},
        {"char16_t", FundamentalTypeKind::Char16},
        {"char32_t", FundamentalTypeKind::Char32},
        {"wchar_t", FundamentalTypeKind::WChar},
        {"short", FundamentalTypeKind::Short},
        {"short int", FundamentalTypeKind::Short},
        {"int short", FundamentalTypeKind::Short},
        {"signed short", FundamentalTypeKind::Short},
        {"short signed", FundamentalTypeKind::Short},
        {"signed short int", FundamentalTypeKind::Short},
        {"signed int short", FundamentalTypeKind::Short},
        {"short signed int", FundamentalTypeKind::Short},
        {"short int signed", FundamentalTypeKind::Short},
        {"int signed short", FundamentalTypeKind::Short},
        {"int short signed", FundamentalTypeKind::Short},
        {"unsigned short", FundamentalTypeKind::UnsignedShort},
        {"short unsigned", FundamentalTypeKind::UnsignedShort},
        {"unsigned short int", FundamentalTypeKind::UnsignedShort},
        {"unsigned int short", FundamentalTypeKind::UnsignedShort},
        {"short unsigned int", FundamentalTypeKind::UnsignedShort},
        {"short int unsigned", FundamentalTypeKind::UnsignedShort},
        {"int unsigned short", FundamentalTypeKind::UnsignedShort},
        {"int short unsigned", FundamentalTypeKind::UnsignedShort},
        {"int", FundamentalTypeKind::Int},
        {"signed", FundamentalTypeKind::Int},
        {"signed int", FundamentalTypeKind::Int},
        {"int signed", FundamentalTypeKind::Int},
        {"unsigned", FundamentalTypeKind::UnsignedInt},
        {"unsigned int", FundamentalTypeKind::UnsignedInt},
        {"int unsigned", FundamentalTypeKind::UnsignedInt},
        {"long", FundamentalTypeKind::Long},
        {"long int", FundamentalTypeKind::Long},
        {"int long", FundamentalTypeKind::Long},
        {"signed long", FundamentalTypeKind::Long},
        {"long signed", FundamentalTypeKind::Long},
        {"signed long int", FundamentalTypeKind::Long},
        {"signed int long", FundamentalTypeKind::Long},
        {"long signed int", FundamentalTypeKind::Long},
        {"long int signed", FundamentalTypeKind::Long},
        {"int signed long", FundamentalTypeKind::Long},
        {"int long signed", FundamentalTypeKind::Long},
        {"unsigned long", FundamentalTypeKind::UnsignedLong},
        {"long unsigned", FundamentalTypeKind::UnsignedLong},
        {"unsigned long int", FundamentalTypeKind::UnsignedLong},
        {"unsigned int long", FundamentalTypeKind::UnsignedLong},
        {"long unsigned int", FundamentalTypeKind::UnsignedLong},
        {"long int unsigned", FundamentalTypeKind::UnsignedLong},
        {"int unsigned long", FundamentalTypeKind::UnsignedLong},
        {"int long unsigned", FundamentalTypeKind::UnsignedLong},
        {"long long", FundamentalTypeKind::LongLong},
        {"long long int", FundamentalTypeKind::LongLong},
        {"long int long", FundamentalTypeKind::LongLong},
        {"int long long", FundamentalTypeKind::LongLong},
        {"signed long long", FundamentalTypeKind::LongLong},
        {"long signed long", FundamentalTypeKind::LongLong},
        {"long long signed", FundamentalTypeKind::LongLong},
        {"signed long long int", FundamentalTypeKind::LongLong},
        {"signed int long long", FundamentalTypeKind::LongLong},
        {"long long signed int", FundamentalTypeKind::LongLong},
        {"long long int signed", FundamentalTypeKind::LongLong},
        {"int signed long long", FundamentalTypeKind::LongLong},
        {"int long long signed", FundamentalTypeKind::LongLong},
        {"unsigned long long", FundamentalTypeKind::UnsignedLongLong},
        {"long long unsigned", FundamentalTypeKind::UnsignedLongLong},
        {"unsigned long long int", FundamentalTypeKind::UnsignedLongLong},
        {"unsigned int long long", FundamentalTypeKind::UnsignedLongLong},
        {"long long unsigned int", FundamentalTypeKind::UnsignedLongLong},
        {"long long int unsigned", FundamentalTypeKind::UnsignedLongLong},
        {"int unsigned long long", FundamentalTypeKind::UnsignedLongLong},
        {"int long long unsigned", FundamentalTypeKind::UnsignedLongLong},
        {"float", FundamentalTypeKind::Float},
        {"double", FundamentalTypeKind::Double},
        {"long double", FundamentalTypeKind::LongDouble}
    };
    for (auto const& [key, value]: map)
    {
        if (key == str)
        {
            kind = value;
            return true;
        }
    }
    return false;
}

bool
makeLong(FundamentalTypeKind& kind) noexcept
{
    if (kind == FundamentalTypeKind::Int)
    {
        kind = FundamentalTypeKind::Long;
        return true;
    }
    if (kind == FundamentalTypeKind::Long)
    {
        kind = FundamentalTypeKind::LongLong;
        return true;
    }
    if (kind == FundamentalTypeKind::UnsignedInt)
    {
        kind = FundamentalTypeKind::UnsignedLong;
        return true;
    }
    if (kind == FundamentalTypeKind::UnsignedLong)
    {
        kind = FundamentalTypeKind::UnsignedLongLong;
        return true;
    }
    if (kind == FundamentalTypeKind::Double)
    {
        kind = FundamentalTypeKind::LongDouble;
        return true;
    }
    return false;
}

bool
makeShort(FundamentalTypeKind& kind) noexcept
{
    if (kind == FundamentalTypeKind::Int)
    {
        kind = FundamentalTypeKind::Short;
        return true;
    }
    if (kind == FundamentalTypeKind::UnsignedInt)
    {
        kind = FundamentalTypeKind::UnsignedShort;
        return true;
    }
    return false;
}

bool
makeSigned(FundamentalTypeKind& kind) noexcept
{
    if (kind == FundamentalTypeKind::Char)
    {
        kind = FundamentalTypeKind::SignedChar;
        return true;
    }
    if (kind == FundamentalTypeKind::Short ||
        kind == FundamentalTypeKind::Int ||
        kind == FundamentalTypeKind::Long ||
        kind == FundamentalTypeKind::LongLong)
    {
        // Already signed, but return true
        // because applying the signed specifier
        // is a valid operation
        return true;
    }
    return false;
}

bool
makeUnsigned(FundamentalTypeKind& kind) noexcept
{
    if (kind == FundamentalTypeKind::Char)
    {
        kind = FundamentalTypeKind::UnsignedChar;
        return true;
    }
    // For signed int types, applying the specifier
    // is valid as long as the type was not already
    // declared with "signed"
    if (kind == FundamentalTypeKind::Short)
    {
        kind = FundamentalTypeKind::UnsignedShort;
        return true;
    }
    if (kind == FundamentalTypeKind::Int)
    {
        kind = FundamentalTypeKind::UnsignedInt;
        return true;
    }
    if (kind == FundamentalTypeKind::Long)
    {
        kind = FundamentalTypeKind::UnsignedLong;
        return true;
    }
    if (kind == FundamentalTypeKind::LongLong)
    {
        kind = FundamentalTypeKind::UnsignedLongLong;
        return true;
    }
    // For already unsigned types, the operation
    // is invalid because the type already used the
    // unsigned specifier.
    return false;
}

bool
makeChar(FundamentalTypeKind& kind) noexcept
{
    if (kind == FundamentalTypeKind::Int)
    {
        // Assumes "int" was declared with "signed"
        kind = FundamentalTypeKind::SignedChar;
        return true;
    }
    if (kind == FundamentalTypeKind::UnsignedInt)
    {
        // Assumes "unsigned int" was declared with "unsigned"
        kind = FundamentalTypeKind::UnsignedChar;
        return true;
    }
    return false;
}

std::strong_ordering
operator<=>(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs)
{
    if (!lhs.valueless_after_move() && !rhs.valueless_after_move())
    {
        auto& lhsRef = *lhs;
        auto& rhsRef = *rhs;
        if (lhsRef.Kind == rhsRef.Kind)
        {
            return visit(lhsRef, detail::VisitCompareFn<TypeInfo>(rhsRef));
        }
        return lhsRef.Kind <=> rhsRef.Kind;
    }
    return lhs.valueless_after_move() ?
               std::strong_ordering::less :
               std::strong_ordering::greater;
}

namespace {
// Get an optional reference to the inner type
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
            if (t.PointeeType)
            {
                return &*t.PointeeType;
            }
        }
        if constexpr(requires { t.ElementType; })
        {
            if (t.ElementType)
            {
                return &*t.ElementType;
            }
        }
        if constexpr(requires { t.ReturnType; })
        {
            if (t.ReturnType)
            {
                return &*t.ReturnType;
            }
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

// Get a pointer to the inner type
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

// Get the innermost type
// If there's an internal type, return it
// If there's no internal type, return the current type
template <class PolymorphicTypeInfoTy>
requires std::same_as<std::remove_cvref_t<PolymorphicTypeInfoTy>, Polymorphic<TypeInfo>>
auto&
innermostTypeImpl(PolymorphicTypeInfoTy&& TI) noexcept
{
    if (TI.valueless_after_move())
    {
        return TI;
    }
    /* optional */ auto inner = innerTypeImpl(*TI);
    if (!inner)
    {
        return TI;
    }
    while (inner)
    {
        /* polymorphic */ auto& ref = inner->get();
        if (ref.valueless_after_move() ||
            ref->isNamed())
        {
            return ref;
        }
        inner = innerTypeImpl(*ref);
    }
    return inner->get();
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

Polymorphic<TypeInfo> const&
innermostType(Polymorphic<TypeInfo> const& TI) noexcept
{
    return innermostTypeImpl(TI);
}

Polymorphic<TypeInfo>&
innermostType(Polymorphic<TypeInfo>& TI) noexcept
{
    return innermostTypeImpl(TI);
}

} // clang::mrdocs
