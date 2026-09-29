//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/TArg.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <algorithm>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Metadata/Type/NamedType.hpp>
#include <mrdocs/Metadata/Type/QualifierKind.hpp>

namespace mrdocs {

namespace detail {

bool
isPlaceholderType(Polymorphic<Type> const& t)
{
    return t->isAuto() ||
        (t->isNamed() &&
         t->asNamed().Name->Identifier.empty());
}

} // namespace detail

std::string_view
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

SymbolID
Type::
namedSymbol() const noexcept
{
    if (!isNamed())
    {
        return SymbolID::invalid;
    }
    auto const* NT = this->asNamedPtr();
    MRDOCS_ASSERT(NT);
    MRDOCS_ASSERT(!NT->Name.valueless_after_move());
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
    if (Type const* inner = innerTypePtr(t))
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
        MRDOCS_ASSERT(!t.ParentType.valueless_after_move());
        writeFullType(*t.ParentType, write);
        write("::");
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

    if (Type const* inner = innerTypePtr(t))
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

std::string
toString(Type const& T,
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


// Custom (not the generic described-enum name): renders the C++ type spelling
// (`unsigned int`, `std::nullptr_t`, ...), used as the written type name.
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
operator<=>(Polymorphic<Type> const& lhs, Polymorphic<Type> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    auto& lhsRef = *lhs;
    auto& rhsRef = *rhs;
    if (lhsRef.Kind == rhsRef.Kind)
    {
        return visit(lhsRef, detail::VisitCompareFn<Type>(rhsRef));
    }
    return lhsRef.Kind <=> rhsRef.Kind;
}

// Defined out of line, not inline in a header: the body resolves `lhs <=>
// rhs`, which drives the visitor comparison and `has_describe_kinds<Type>`.
// This translation unit includes Type.hpp, so the kinds are already
// registered here; a header-inline body could be parsed before the
// registration and cache the trait as false (Clang <= 19).
bool
operator==(Polymorphic<Type> const& lhs, Polymorphic<Type> const& rhs)
{
    return std::is_eq(lhs <=> rhs);
}

namespace {
// Get an optional reference to the inner type
template <
    class TypeTy,
    bool isMutable = !std::is_const_v<std::remove_reference_t<TypeTy>>,
    class Ptr = std::conditional_t<isMutable, Polymorphic<Type>*, Polymorphic<Type> const*>,
    class Ref = std::conditional_t<isMutable, Polymorphic<Type>&, Polymorphic<Type> const&>>
requires std::same_as<std::remove_cvref_t<TypeTy>, Type>
Optional<Ref>
innerTypeImpl(TypeTy&& TI) noexcept
{
    // Get a pointer to the inner type
    Ptr innerPtr = visit(TI, []<typename T>(T& t) -> Ptr
    {
        if constexpr(requires { t.PointeeType; })
        {
            MRDOCS_ASSERT(!t.PointeeType.valueless_after_move());
            return &t.PointeeType;
        }
        if constexpr(requires { t.ElementType; })
        {
            MRDOCS_ASSERT(!t.ElementType.valueless_after_move());
            return &t.ElementType;
        }
        if constexpr(requires { t.ReturnType; })
        {
            MRDOCS_ASSERT(!t.ReturnType.valueless_after_move());
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

// Get a pointer to the inner type
template <
    class TypeTy,
    bool isMutable = !std::is_const_v<std::remove_reference_t<TypeTy>>,
    class Ptr = std::conditional_t<isMutable, Polymorphic<Type>*, Polymorphic<Type> const*>,
    class Ref = std::conditional_t<isMutable, Polymorphic<Type>&, Polymorphic<Type> const&>,
    class InnerPtr = std::conditional_t<isMutable, Type*, Type const*>>
requires std::same_as<std::remove_cvref_t<TypeTy>, Type>
InnerPtr
innerTypePtrImpl(TypeTy&& TI) noexcept
{
    Optional<Ref> res = innerTypeImpl(TI);
    if (res)
    {
        MRDOCS_ASSERT(!res->valueless_after_move());
        return &**res;
    }
    return nullptr;
}

// Walk to the deepest nested type, preserving constness.
template <
    class PolyRef,
    bool isMutable = !std::is_const_v<std::remove_reference_t<PolyRef>>,
    class Ref = std::conditional_t<isMutable, Polymorphic<Type>&, Polymorphic<Type> const&>>
requires std::same_as<std::remove_cvref_t<PolyRef>, Polymorphic<Type>>
Ref
innermostTypeImpl(PolyRef&& TI) noexcept
{
    auto* current = std::addressof(TI);
    Optional<Ref> inner = innerTypeImpl(**current);
    while (inner)
    {
        current = std::addressof(*inner);
        if (current->valueless_after_move())
            break;
        if ((*current)->isNamed())
            break;
        inner = innerTypeImpl(**current);
    }
    return *current;
}

} // namespace

Optional<Polymorphic<Type> const&>
innerType(Type const& TI) noexcept
{
    return innerTypeImpl<Type const&>(TI);
}

Optional<Polymorphic<Type>&>
innerType(Type& TI) noexcept
{
    return innerTypeImpl<Type&>(TI);
}

Type const*
innerTypePtr(Type const& TI) noexcept
{
    return innerTypePtrImpl<Type const&>(TI);
}

Type*
innerTypePtr(Type& TI) noexcept
{
    return innerTypePtrImpl<Type&>(TI);
}

Polymorphic<Type> const&
innermostType(Polymorphic<Type> const& TI) noexcept
{
    return innermostTypeImpl<Polymorphic<Type> const&>(TI);
}

Polymorphic<Type>&
innermostType(Polymorphic<Type>& TI) noexcept
{
    return innermostTypeImpl<Polymorphic<Type>&>(TI);
}

namespace {

template <bool isInner>
bool
isDecayedEqualImpl(
    Optional<Polymorphic<Type>> const& lhs,
    Optional<Polymorphic<Type>> const& rhs,
    NameEquality const& sameName);


// Check if two types are equal after decay
//
// The isInner template parameter indicates if
// we are comparing inner types (e.g., pointee types)
// or root types (e.g., function parameter types) because
// the rules are slightly different depending
// on the level of the type specifiers.
//
template <bool isInner>
bool
isDecayedEqualImpl(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    NameEquality const& sameName)
{
    // Polymorphic
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    // Type
    bool const decayToPointer = !isInner && (lhs->isArray() || rhs->isArray());
    if (!decayToPointer)
    {
        MRDOCS_CHECK_OR(lhs->Kind == rhs->Kind, false);
    }
    else
    {
        // in root types, arrays are decayed to pointers
        MRDOCS_CHECK_OR(lhs->isArray() || lhs->isPointer(), false);
        MRDOCS_CHECK_OR(rhs->isArray() || rhs->isPointer(), false);
    }
    MRDOCS_CHECK_OR(lhs->IsPackExpansion == rhs->IsPackExpansion, false);
    if constexpr (isInner)
    {
        // const and volatile are ignored from root types
        // in function parameters
        MRDOCS_CHECK_OR(lhs->IsConst == rhs->IsConst, false);
        MRDOCS_CHECK_OR(lhs->IsVolatile == rhs->IsVolatile, false);
    }
    MRDOCS_CHECK_OR(lhs->Constraints == rhs->Constraints, false);
    switch (lhs->Kind)
    {
    // Types that never decay are compared directly, but we
    // only compare the fields of the type, without reevaluating
    // the fields of Type.
    case TypeKind::Named:
    {
        Name const& lhsName = *lhs->asNamed().Name;
        Name const& rhsName = *rhs->asNamed().Name;
        MRDOCS_CHECK_OR(sameName(lhsName, rhsName), false);
        // Specializations of one template are distinct types unless their
        // template arguments agree as well.
        MRDOCS_CHECK_OR(
            lhsName.isSpecialization() == rhsName.isSpecialization(), false);
        if (!lhsName.isSpecialization())
        {
            return true;
        }
        auto const& lhsArgs = lhsName.asSpecialization().TemplateArgs;
        auto const& rhsArgs = rhsName.asSpecialization().TemplateArgs;
        MRDOCS_CHECK_OR(lhsArgs.size() == rhsArgs.size(), false);
        return std::ranges::equal(
            lhsArgs, rhsArgs,
            [&](Polymorphic<TArg> const& a, Polymorphic<TArg> const& b)
            {
                return isEqual(a, b, sameName);
            });
    }
    case TypeKind::Decltype:
    {
        return lhs->asDecltype().Operand ==
               rhs->asDecltype().Operand;
    }
    case TypeKind::Auto:
    {
        auto const& lhsAuto = lhs->asAuto();
        auto const& rhsAuto = rhs->asAuto();
        return lhsAuto.Keyword == rhsAuto.Keyword &&
               lhsAuto.Constraint == rhsAuto.Constraint;
    }
    case TypeKind::LValueReference:
    {
        return
            isDecayedEqualImpl<true>(
                lhs->asLValueReference().PointeeType,
                rhs->asLValueReference().PointeeType,
                sameName);
    }
    case TypeKind::RValueReference:
    {
        return
            isDecayedEqualImpl<true>(
                dynamic_cast<RValueReferenceType const&>(*lhs).PointeeType,
                dynamic_cast<RValueReferenceType const&>(*rhs).PointeeType,
                sameName);
    }
    case TypeKind::MemberPointer:
    {
        auto const& lhsMP = dynamic_cast<MemberPointerType const&>(*lhs);
        auto const& rhsMP = dynamic_cast<MemberPointerType const&>(*rhs);
        return
            isDecayedEqualImpl<true>(lhsMP.PointeeType, rhsMP.PointeeType, sameName) &&
            isDecayedEqualImpl<true>(lhsMP.ParentType, rhsMP.ParentType, sameName);
    }
    case TypeKind::Function:
    {
        auto const& lhsF = dynamic_cast<FunctionType const&>(*lhs);
        auto const& rhsF = dynamic_cast<FunctionType const&>(*rhs);
        MRDOCS_CHECK_OR(lhsF.RefQualifier == rhsF.RefQualifier, false);
        MRDOCS_CHECK_OR(lhsF.ExceptionSpec == rhsF.ExceptionSpec, false);
        MRDOCS_CHECK_OR(lhsF.IsVariadic == rhsF.IsVariadic, false);
        MRDOCS_CHECK_OR(isDecayedEqualImpl<true>(lhsF.ReturnType, rhsF.ReturnType, sameName), false);
        MRDOCS_CHECK_OR(lhsF.ParamTypes.size() == rhsF.ParamTypes.size(), false);
        for (std::size_t i = 0; i < lhsF.ParamTypes.size(); ++i)
        {
            MRDOCS_CHECK_OR(isDecayedEqualImpl<false>(lhsF.ParamTypes[i], rhsF.ParamTypes[i], sameName), false);
        }
        return true;
    }
    // Types that should decay
    case TypeKind::Pointer:
    case TypeKind::Array:
    {
        auto const I1 = innerType(*lhs);
        auto const I2 = innerType(*rhs);
        // Both inner types must be present or absent, otherwise not equal
        MRDOCS_CHECK_OR(static_cast<bool>(I1) == static_cast<bool>(I2), false);
        // Both inner types are absent: they are equal
        MRDOCS_CHECK_OR(static_cast<bool>(I1) && static_cast<bool>(I2), true);
        // Both inner types are present: compare them internally
        return isDecayedEqualImpl<true>(*I1, *I2, sameName);
    }
    default:
        MRDOCS_UNREACHABLE();
    }
    return true;
}

template <bool isInner>
bool
isDecayedEqualImpl(
    Optional<Polymorphic<Type>> const& lhs,
    Optional<Polymorphic<Type>> const& rhs,
    NameEquality const& sameName)
{
    MRDOCS_CHECK_OR(static_cast<bool>(lhs) == static_cast<bool>(rhs), false);
    MRDOCS_CHECK_OR(static_cast<bool>(lhs) && static_cast<bool>(rhs), true);
    return isDecayedEqualImpl<isInner>(*lhs, *rhs, sameName);
}

} // (anon)

bool
isDecayedEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    NameEquality const& sameName)
{
    return isDecayedEqualImpl<false>(lhs, rhs, sameName);
}

bool
isEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    NameEquality const& sameName)
{
    return isDecayedEqualImpl<true>(lhs, rhs, sameName);
}

bool
isDecayedEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs)
{
    return isDecayedEqual(lhs, rhs, isSameName);
}

bool
isEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs)
{
    return isEqual(lhs, rhs, isSameName);
}


} // mrdocs
