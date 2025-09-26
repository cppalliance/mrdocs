//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/AST/ParseRef.hpp>
#include <mrdocs/Metadata/Info/Function.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <mrdocs/Support/Report.hpp>
#include <llvm/ADT/StringExtras.h>
#include <format>

namespace clang::mrdocs {

namespace {
constexpr
bool
isDigit(char const c)
{
    return c >= '0' && c <= '9';
}

constexpr
bool
isIdentifierStart(char const c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr
bool
isIdentifierContinuation(char const c)
{
    return isIdentifierStart(c) || isDigit(c);
}

/* Holds information about a parsed function suffix during reference parsing.

   Used internally by RefParser to accumulate details about function parameters,
   variadic status, and exception specifications while parsing C++ symbol
   references.

   Example: In 'void foo(int, double, ...)', Params holds 'int' and 'double',
   IsVariadic is true, HasVoid is false.
*/
struct ParsedFunctionSuffix
{
    /* List of parsed function parameter types.
       Example: For 'void foo(int, double)', Params contains 'int' and 'double'.
    */
    llvm::SmallVector<Polymorphic<TypeInfo>, 8> Params;

    /* True if the parameter list contains only 'void'.
       Example: For 'void foo(void)', HasVoid is true.
    */
    bool HasVoid{false};

    /* True if the function is variadic (contains ...).
       Example: For 'void foo(int, ...)', IsVariadic is true.
    */
    bool IsVariadic{false};

    /* Exception specification for the function.
       Example: For 'void foo() noexcept', ExceptionSpec holds 'noexcept'.
    */
    NoexceptInfo ExceptionSpec;

    /* Virtual destructor for safe polymorphic deletion. */
    virtual ~ParsedFunctionSuffix() = default;
};

struct ParsedMemberFunctionSuffix
    : ParsedFunctionSuffix
{
    bool IsConst{false};
    bool IsVolatile{false};
    ReferenceKind Kind{ReferenceKind::None};
    bool IsExplicitObjectMemberFunction{false};
};

class RefParser
{
    char const* first_;
    char const* ptr_;
    char const* last_;
    ParsedRef& result_;
    std::string error_msg_;
    char const* error_pos_{nullptr};

public:
    explicit
    RefParser(
        char const* first,
        char const* last,
        ParsedRef& result) noexcept
        : first_(first),
          ptr_(first),
          last_(last),
          result_(result)
    {}

    bool
    parse()
    {
        skipWhitespace();
        if (parseLiteral("::"))
        {
            result_.IsFullyQualified = true;
        }
        MRDOCS_CHECK_OR(parseComponents(result_.Components), false);
        result_.HasFunctionParameters = peek('(', ' ');
        if (result_.HasFunctionParameters)
        {
            ParsedMemberFunctionSuffix functionParameters;
            MRDOCS_CHECK_OR(parseFunctionSuffix(functionParameters), false);
            result_.FunctionParameters = std::move(functionParameters.Params);
            result_.IsVariadic = functionParameters.IsVariadic;
            result_.ExceptionSpec = std::move(functionParameters.ExceptionSpec);
            result_.IsConst = functionParameters.IsConst;
            result_.IsVolatile = functionParameters.IsVolatile;
            result_.Kind = functionParameters.Kind;
            result_.IsExplicitObjectMemberFunction = functionParameters.IsExplicitObjectMemberFunction;
        }
        error_msg_.clear();
        error_pos_ = nullptr;
        return true;
    }

    Error
    error() const
    {
        return Error(error_msg_);
    }

    char const*
    errorPos() const
    {
        return error_pos_;
    }

    char const*
    position() const
    {
        return ptr_;
    }

private:
    void
    setError(char const* pos, std::string_view str)
    {
        // Only set the error if it's not already set
        // with a more specific error message
        if (!error_pos_ || error_msg_.empty())
        {
            error_msg_ = std::string(str);
            error_pos_ = pos;
        }
    }

    void
    setError(std::string_view str)
    {
        setError(ptr_, str);
    }

    // Check if [ptr_, last_) starts with the literal `lit`
    bool
    parseLiteral(std::string_view lit)
    {
        if (std::cmp_greater(lit.size(), last_ - ptr_))
        {
            return false;
        }
        if (std::equal(lit.begin(), lit.end(), ptr_))
        {
            ptr_ += lit.size();
            return true;
        }
        return false;
    }

    bool
    parseLiteral(char lit)
    {
        if (!hasMore())
        {
            return false;
        }
        if (*ptr_ == lit)
        {
            ++ptr_;
            return true;
        }
        return false;
    }

    bool
    parseAnyLiteral(std::initializer_list<std::string_view> lits)
    {
        for (std::string_view const lit : lits)
        {
            if (parseLiteral(lit))
            {
                return true;
            }
        }
        return false;
    }

    bool
    parseKeyword(std::string_view lit)
    {
        char const* start = ptr_;
        if (!parseLiteral(lit))
        {
            return false;
        }
        if (peek(isIdentifierContinuation))
        {
            ptr_ = start;
            return false;
        }
        return true;
    }

    bool
    parseAnyKeyword(std::initializer_list<std::string_view> lits)
    {
        for (std::string_view const lit : lits)
        {
            if (parseKeyword(lit))
            {
                return true;
            }
        }
        return false;
    }

    bool
    peek(char c)
    {
        return ptr_ != last_ && *ptr_ == c;
    }

    bool
    peek(std::string_view str) {
        if (std::cmp_greater(str.size(), last_ - ptr_))
        {
            return false;
        }
        return std::equal(str.begin(), str.end(), ptr_);
    }

    template <std::invocable<char> F>
    bool
    peek(F f)
    {
        return ptr_ != last_ && f(*ptr_);
    }

    bool
    peek(char c, char skip)
    {
        char const* first = ptr_;
        while (first != last_ && *first == skip)
        {
            ++first;
        }
        return first != last_ && *first == c;
    }

    bool
    peek(std::string_view str, char skip)
    {
        char const* first = ptr_;
        while (first != last_ && *first == skip)
        {
            ++first;
        }
        if (std::cmp_greater(str.size(), last_ - first))
        {
            return false;
        }
        return std::equal(str.begin(), str.end(), first);
    }

    bool
    peekBack(char c, char skip)
    {
        if (ptr_ == first_)
        {
            return false;
        }
        char const* last = ptr_;
        last--;
        while (last != first_ && *last == skip)
        {
            --last;
        }
        return last != first_ && *last == c;
    }

    bool
    peekAny(std::initializer_list<char> chars)
    {
        return ptr_ != last_ && contains(chars, *ptr_);
    }

    bool
    peekAny(std::initializer_list<char> chars, char skip)
    {
        char const* first = ptr_;
        while (first != last_ && *first == skip)
        {
            ++first;
        }
        return first != last_ && contains(chars, *first);
    }

    bool
    advance(char c)
    {
        char const* start = ptr_;
        while (ptr_ != last_ && *ptr_ == c)
        {
            ++ptr_;
        }
        return ptr_ != start;
    }

    bool
    rewindUntil(char const c)
    {
        if (first_ == last_)
        {
            return false;
        }
        if (ptr_ == last_)
        {
            --ptr_;
        }
        while (ptr_ != first_ && *ptr_ != c)
        {
            --ptr_;
        }
        return *ptr_ == c;
    }

    bool
    parseComponents(llvm::SmallVector<ParsedRefComponent, 8>& components)
    {
        char const* start = ptr_;
        while (true)
        {
            char const* compStart = ptr_;
            if (!parseComponent(result_.Components.emplace_back()))
            {
                return false;
            }
            if (!peek("::", ' '))
            {
                return !components.empty();
            }
            skipWhitespace();
            parseLiteral("::");
            // If we have a "::" separator, so this is not
            // the last component. Check the rules for
            // nested-name-specifier
            ParsedRefComponent const& comp = components.back();
            if (comp.isOperator())
            {
                ptr_ = compStart;
                setError("operator '::' is not allowed in nested-name-specifier");
                ptr_ = start;
                return false;
            }
            if (comp.isConversion())
            {
                ptr_ = compStart;
                setError("conversion operator is not allowed in nested-name-specifier");
                ptr_ = start;
                return false;
            }
        }
    }

    bool
    parseComponent(ParsedRefComponent& c)
    {
        if (!hasMore())
        {
            setError("expected component name");
            return false;
        }
        char const *start = ptr_;
        skipWhitespace();
        if (!parseUnqualifiedId(c))
        {
            setError("expected component name");
            ptr_ = start;
            return false;
        }
        if (peek('<', ' ')) {
            skipWhitespace();
            if (!parseTemplateArguments(c.TemplateArguments))
            {
                setError("expected template arguments");
                ptr_ = start;
                return false;
            }
        }
        return true;
    }

    bool
    parseUnqualifiedId(ParsedRefComponent& c)
    {
        // https://en.cppreference.com/w/cpp/language/identifiers#Unqualified_identifiers
        // Besides suitably declared identifiers, the following unqualified identifier
        // expressions can be used in expressions in the same role:
        // - an overloaded operator name in function notation, such as operator+ or operator new;
        // - a user-defined conversion function name, such as operator bool;
        // - a user-defined literal operator name, such as operator "" _km;
        // - a template name followed by its argument list, such as MyTemplate<int>;
        // - the character ~ followed by a class name, such as ~MyClass;
        // - the character ~ followed by a decltype specifier, such as ~decltype(str).
        // - the character ~ followed by a pack indexing specifier, such as ~pack...[0].
        char const* start = ptr_;

        if (!hasMore())
        {
            setError("expected component name");
            return false;
        }

        // Try to parse as an operator
        if (parseOperator(c))
        {
            return true;
        }

        // Parse conversion operator
        if (parseConversionOperator(c))
        {
            return true;
        }

        // Parse as a regular identifier
        if (!parseDestructorOrIdentifier(c.Name))
        {
            setError("expected component name");
            ptr_ = start;
            return false;
        }
        c.Operator = OperatorKind::None;
        return true;
    }

    bool
    parseConversionOperator(ParsedRefComponent& c)
    {
        char const* start = ptr_;
        if (!parseKeyword("operator"))
        {
            return false;
        }
        skipWhitespace();
        Polymorphic<TypeInfo> conversionType = nullable_traits<Polymorphic<TypeInfo>>::null();
        if (!parseDeclarationSpecifier(conversionType) ||
            conversionType.valueless_after_move())
        {
            ptr_ = start;
            return false;
        }
        c.ConversionType = std::move(conversionType);
        return true;
    }

    bool
    parseDestructorOrIdentifier(std::string_view& s)
    {
        // A regular identifier or a function name
        char const* start = ptr_;
        skipWhitespace();
        if (parseLiteral("~"))
        {
            skipWhitespace();
        }
        if (parseKeyword("operator"))
        {
            setError("'operator' is an invalid identifier");
            ptr_ = start;
            return false;
        }
        if (!parseIdentifier(true))
        {
            ptr_ = start;
            return false;
        }
        s = std::string_view(start, ptr_ - start);
        return true;
    }

    bool
    parseIdentifier(bool const allowTemplateDisambiguation)
    {
        // https://en.cppreference.com/w/cpp/language/identifiers
        char const* start = ptr_;
        skipWhitespace();
        if (!hasMore())
        {
            setError("end of string: expected identifier");
            ptr_ = start;
            return false;
        }
        if (allowTemplateDisambiguation)
        {
            if (parseAnyKeyword({"template", "typedef"}))
            {
                skipWhitespace();
            }
        }
        if (isIdentifierStart(*ptr_))
        {
            ++ptr_;
        }
        else
        {
            setError("invalid identifier start character");
            ptr_ = start;
            return false;
        }
        while (isIdentifierContinuation(*ptr_))
        {
            ++ptr_;
        }
        return true;
    }

    bool
    parseOperator(ParsedRefComponent& c)
    {
        const char* start = ptr_;
        if (!parseLiteral("operator"))
        {
            return false;
        }
        skipWhitespace();

        // Try to handle operators that would conflict with the "<(" separators first
        static constexpr std::string_view conflictingOperators[] = {
            "()", "<=>", "<<=", "<<", "<=", "<"
        };
        for (std::string_view const op : conflictingOperators)
        {
            if (parseLiteral(op))
            {
                c.Operator = getOperatorKindFromSuffix(op);
                MRDOCS_ASSERT(c.Operator != OperatorKind::None);
                c.Name = getOperatorName(c.Operator, true);
                return true;
            }
        }

        // Handle other operator types by looking at the first
        // character equal to "<(.:"
        char const* op_start = ptr_;
        while (hasMore())
        {
            if (*ptr_ == '<' || *ptr_ == '(' || *ptr_ == '.' || *ptr_ == ':' || *ptr_ == ' ')
            {
                break;
            }
            ++ptr_;
        }
        if (ptr_ == op_start)
        {
            setError("expected operator specifier");
            ptr_ = start;
            return false;
        }
        std::string_view op(op_start, ptr_ - op_start);
        c.Operator = getOperatorKindFromSuffix(op);
        if (c.Operator == OperatorKind::None)
        {
            // This operator doesn't exist
            ptr_ = start;
            return false;
        }
        c.Name = getOperatorName(c.Operator, true);
        return true;
    }

    bool
    parseTemplateArguments(std::vector<Polymorphic<TArg>>& TemplateArguments)
    {
        // https://en.cppreference.com/w/cpp/language/template_parameters#Template_arguments
        char const* start = ptr_;
        if (!parseLiteral('<'))
        {
            ptr_ = start;
            return false;
        }
        skipWhitespace();
        // Add an empty slot for the first template argument
        TemplateArguments.emplace_back(nullable_traits<Polymorphic<TArg>>::null());
        while (parseTemplateArgument(TemplateArguments.back()))
        {
            skipWhitespace();
            if (parseLiteral(','))
            {
                skipWhitespace();
                // Add another empty slot for the next argument after each comma
                // This allows parseTemplateArgument to fill the new slot
                TemplateArguments.emplace_back(nullable_traits<Polymorphic<TArg>>::null());
            }
            else
            {
                break;
            }
        }
        skipWhitespace();
        if (!parseLiteral('>'))
        {
            setError("expected '>'");
            ptr_ = start;
            return false;
        }
        return true;
    }

    bool
    parseTemplateArgument(Polymorphic<TArg>& dest)
    {
        // https://en.cppreference.com/w/cpp/language/template_parameters#Template_arguments
        // If an argument can be interpreted as both a type-id and an
        // expression, it is always interpreted as a type-id, even if the
        // corresponding template parameter is non-type:
        if (!hasMore())
        {
            return false;
        }
        skipWhitespace();
        char const* start = ptr_;
        Polymorphic<TypeInfo> type = nullable_traits<Polymorphic<TypeInfo>>::null();
        if (parseTypeId(type))
        {
            dest = Polymorphic<TArg>(std::in_place_type<TypeTArg>);
            dynamic_cast<TypeTArg &>(*dest).Type = std::move(type);
            return true;
        }

        // If the argument is not a type-id, it is an expression
        // The expression is internally balanced regarding '<'
        // and '>' and ends with a comma
        char const* exprStart = ptr_;
        while (parseBalanced("<", ">", {",", ">"}))
        {
            if (!peekAny({',', '>'}, ' '))
            {
                continue;
            }
            break;
        }
        if (ptr_ == exprStart)
        {
            setError("expected template argument");
            ptr_ = start;
            return false;
        }
        dest = Polymorphic<TArg>(std::in_place_type<NonTypeTArg>);
        static_cast<NonTypeTArg &>(*dest).Value.Written =
            trim(std::string_view(exprStart, ptr_ - exprStart));
        return true;
    }

    bool
    parseFunctionSuffix(ParsedFunctionSuffix& dest)
    {
        // parameter-list:
        // https://en.cppreference.com/w/cpp/language/function#Parameter_list
        // possibly empty, comma-separated list of the function parameters,
        // where a function parameter is:
        // "void", or
        // attr? this? decl-specifier-seq [declarator|abstract-declarator] [= initializer]?
        //
        // So, for purposes of a documentation ref, we only need:
        // "void"
        // this? decl-specifier-seq

        char const* start = ptr_;
        skipWhitespace();
        if (!parseLiteral('('))
        {
            ptr_ = start;
            return false;
        }
        skipWhitespace();
        while (hasMore() && !peek(')'))
        {
            if (!parseFunctionParameter(dest))
            {
                setError("expected function parameter");
                ptr_ = start;
                return false;
            }
            skipWhitespace();
            if (parseLiteral(','))
            {
                skipWhitespace();
            }
            else
            {
                break;
            }
        }
        skipWhitespace();
        if (!parseLiteral(')'))
        {
            setError("expected ')'");
            ptr_ = start;
            return false;
        }

        if (!parseFunctionQualifiers(dest))
        {
            setError("invalid function qualifiers");
            ptr_ = start;
            return false;
        }

        return true;
    }

    bool
    parseFunctionParameter(ParsedFunctionSuffix& dest)
    {
        if (!hasMore())
        {
            return false;
        }
        char const* start = ptr_;

        // Void parameter: accepted, but doesn't need to be stored
        skipWhitespace();
        char const* voidStart = ptr_;
        if (parseKeyword("void"))
        {
            skipWhitespace();
            if (peekAny({',', ')'}))
            {
                if (dest.Params.size() != 0)
                {
                    ptr_ = voidStart;
                    setError("expected 'void' to be the only parameter");
                    ptr_ = start;
                    return false;
                }
                if (dest.HasVoid)
                {
                    ptr_ = voidStart;
                    setError("multiple 'void' parameters");
                    ptr_ = start;
                    return false;
                }
                dest.HasVoid = true;
                skipWhitespace();
                return true;
            }
            ptr_ = start;
            skipWhitespace();
        }

        // Variadic parameter: accepted, but doesn't need to be stored
        // in the parameter list.
        if (parseLiteral("..."))
        {
            skipWhitespace();
            dest.IsVariadic = true;
            return true;
        }

        // Empty parameter
        if (peekAny({',', ')'}))
        {
            setError("expected parameter type");
            ptr_ = start;
            return false;
        }

        // Parse as a regular parameter
        // https://en.cppreference.com/w/cpp/language/function#Parameter_list
        // this? decl-specifier-seq [declarator/abstract-declarator]?

        // "this" parameter: accepted, but doesn't need to be stored
        // in the parameter list
        if (auto* destMF = dynamic_cast<ParsedMemberFunctionSuffix*>(&dest);
            destMF &&
            parseKeyword("this"))
        {
            if (!dest.Params.empty())
            {
                setError("expected 'this' to be the first parameter");
                ptr_ = start;
                return false;
            }
            destMF->IsExplicitObjectMemberFunction = true;
            skipWhitespace();
        }

        // https://en.cppreference.com/w/cpp/language/function#Parameter_list
        // decl-specifier-seq
        auto &curParam = dest.Params.emplace_back(nullable_traits<Polymorphic<TypeInfo>>::null());
        if (!parseTypeId(curParam))
        {
            ptr_ = start;
            setError("expected type-id");
            return false;
        }

        // 2. After determining the type of each parameter, any parameter
        // of type “array of T” or of function type T is adjusted to be
        // “pointer to T”.
        // https://en.cppreference.com/w/cpp/language/function#Function_type
        if (curParam->isArray())
        {
            ArrayTypeInfo PrevParamType = dynamic_cast<ArrayTypeInfo &>(*curParam);
            curParam = Polymorphic<TypeInfo>(std::in_place_type<PointerTypeInfo>);
            auto& curAsPointerType = dynamic_cast<PointerTypeInfo &>(*curParam);
            curAsPointerType.PointeeType = std::move(PrevParamType.ElementType);
            auto& PrevAsBase = dynamic_cast<TypeInfo&>(PrevParamType);
            *curParam = std::move(PrevAsBase);
        }

        // 3. After producing the list of parameter types, any top-level
        // cv-qualifiers modifying a parameter type are deleted when
        // forming the function type.
        // https://en.cppreference.com/w/cpp/language/function#Function_type
        curParam->IsConst = false;
        curParam->IsVolatile = false;

        skipWhitespace();
        return true;
    }

    bool
    parseTypeId(Polymorphic<TypeInfo>& dest)
    {
        char const* start = ptr_;

        // https://en.cppreference.com/w/cpp/language/function#Parameter_list
        // decl-specifier-seq
        if (!parseDeclarationSpecifiers(dest))
        {
            ptr_ = start;
            setError("expected parameter qualified type");
            return false;
        }

        // If a parameter is not used in the function body, it does
        // not need to be named (it's sufficient to use an
        // abstract declarator).
        // MrDocs refs only use abstract declarators. Any parameter
        // name is ignored.
        if (!parseAbstractDeclarator(dest))
        {
            setError("expected abstract declarator");
            ptr_ = start;
            return false;
        }

        return true;
    }

    bool
    parseDeclarationSpecifiers(Polymorphic<TypeInfo>& dest)
    {
        //static constexpr std::string_view typeQualifiers[] = {
        //    "const", "volatile"
        //};
        static constexpr std::string_view typeModifiers[] = {
            "long", "short", "signed", "unsigned"
        };
        static constexpr std::string_view typeSpecifiers[] = {
            "long", "short", "signed", "unsigned", "const", "volatile"
        };

        // https://en.cppreference.com/w/cpp/language/declarations#Specifiers
        // decl-specifier-seq is a sequence whitespace-separated decl-specifier's
        char const* start = ptr_;
        llvm::SmallVector<std::string_view, 8> specifiers;
        while (true)
        {
            skipWhitespace();
            char const* specStart = ptr_;
            if (peekAny({',', ')', '&'}))
            {
                break;
            }
            if (!parseDeclarationSpecifier(dest))
            {
                // This could be the end of the specifiers, followed
                // by declarators, or an error. We need to check if
                // dest was properly set.
                // If dest was not set, we need to return an error.
                // If we have one of the "long", "short", "signed", "unsigned"
                // specifiers, then we don't have an error because
                // we can later infer the type from these.
                if (dest.valueless_after_move() &&
                    !contains_any(specifiers, typeModifiers))
                {
                    setError(specStart, "expected declaration specifier");
                    ptr_ = start;
                    return false;
                }
                // Clear the error and let the type modifiers set `dest`
                error_msg_.clear();
                error_pos_ = nullptr;
                break;
            }
            auto const specifierStr =
                trim(std::string_view(specStart, ptr_ - specStart));
            if (contains(typeSpecifiers, specifierStr))
            {
                specifiers.push_back(specifierStr);
            }
            if (!skipWhitespace())
            {
                break;
            }
        }
        if (dest.valueless_after_move() && specifiers.empty())
        {
            // We need at least one type declarator or specifier
            ptr_ = start;
            return false;
        }

        // Look for conflicting specifiers
        if (specifiers.size() > 1)
        {
            // If we already have a declaration specifier, we need to
            // check if we have a valid sequence of specifiers:
            // - In general, only one type specifier is allowed
            // - "const" and "volatile" can be combined with any type specifier
            //    except itself
            if (contains_n(specifiers, "const", 2))
            {
                setError(start, "multiple 'const' specifiers");
                ptr_ = start;
                return false;
            }

            if (contains_n(specifiers, "volatile", 2))
            {
                setError(start, "multiple 'volatile' specifiers");
                ptr_ = start;
                return false;
            }

            // - "signed" or "unsigned" can be combined with "char", "long", "short", or "int".
            if (contains_n_any(specifiers, {"signed", "unsigned"}, 2))
            {
                setError(start, "multiple 'signed' or 'unsigned' specifiers");
                ptr_ = start;
                return false;
            }

            // - "short" or "long" can be combined with int.
            // - "long" can be combined with "double" and "long"
            // "short" is allowed only once
            // "long" is allowed twice
            if (contains_n(specifiers, "short", 2))
            {
                setError(start, "too many 'short' specifiers");
                ptr_ = start;
                return false;
            }

            if (contains(specifiers, "short") &&
                contains(specifiers, "long"))
            {
                setError(start, "cannot combine 'short' with 'long'");
                ptr_ = start;
                return false;
            }

            if (contains_n(specifiers, "long", 3))
            {
                setError(start, "too many 'long' specifiers");
                ptr_ = start;
                return false;
            }
        }

        // "signed" or "unsigned" can be combined with "char", "long", "short", or "int".
        if (contains_any(specifiers, {"signed", "unsigned"}))
        {
            bool explicitlySigned = contains(specifiers, "signed");
            std::string_view signStr = explicitlySigned ? "signed" : "unsigned";
            // Infer basic fundamental type from "signed" or "unsigned",
            // which is "int"
            if (dest.valueless_after_move())
            {
                dest = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
                auto &NTI = dynamic_cast<NamedTypeInfo &>(*dest);
                NTI.Name->Name = "int";
                NTI.FundamentalType = FundamentalTypeKind::Int;
            }
            // Check if the type is named
            if (!dest->isNamed())
            {
              setError(std::format("expected named type for '{}' specifier",
                                   signStr));
              ptr_ = start;
              return false;
            }
            // Check if the type is "int" or "char"
            auto& namedParam = dynamic_cast<NamedTypeInfo&>(*dest);
            if (!namedParam.FundamentalType)
            {
              setError(std::format(
                  "expected fundamental type for '{}' specifier", signStr));
              ptr_ = start;
              return false;
            }
            bool promoted =
                explicitlySigned ?
                    makeSigned(*namedParam.FundamentalType) :
                    makeUnsigned(*namedParam.FundamentalType);
            if (!promoted)
            {
              setError(std::format(
                  "expected 'int' or 'char' for '{}' specifier", signStr));
              ptr_ = start;
              return false;
            }
            // Add the specifier to the type name
            namedParam.Name->Name = toString(*namedParam.FundamentalType);
        }

        // - "short" can be combined with int.
        if (contains(specifiers, "short"))
        {
            // Infer basic fundamental type for "short", which is "int"
            if (dest.valueless_after_move())
            {
                dest = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
                auto &NTI = dynamic_cast<NamedTypeInfo &>(*dest);
                NTI.Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);
                NTI.Name->Name = "int";
                NTI.FundamentalType = FundamentalTypeKind::Int;
            }
            // Check if the type is named
            if (!dest->isNamed())
            {
                setError(start, "expected named type for 'short' specifier");
                ptr_ = start;
                return false;
            }

            // Check if the type is "int"
            auto& namedParam = dynamic_cast<NamedTypeInfo&>(*dest);
            if (!namedParam.FundamentalType)
            {
                setError(start, "expected fundamental type for 'short' specifier");
                ptr_ = start;
                return false;
            }
            if (bool promoted = makeShort(*namedParam.FundamentalType);
                !promoted)
            {
                setError(start, "expected 'int' for 'short' specifier");
                ptr_ = start;
                return false;
            }
            // Add the specifier to the type name
            namedParam.Name->Name = toString(*namedParam.FundamentalType);
        }

        // - "long" can be combined with "int", "double" and "long"
        if (contains(specifiers, "long"))
        {
            // Infer basic fundamental type for "long", which is "int"
            if (dest.valueless_after_move())
            {
                dest = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
                auto &NTI = dynamic_cast<NamedTypeInfo &>(*dest);
                NTI.Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);
                NTI.Name->Name = "int";
                NTI.FundamentalType = FundamentalTypeKind::Int;
            }
            // Check if the type is named
            if (!dest->isNamed())
            {
                setError(start, "expected named type for 'long' specifier");
                ptr_ = start;
                return false;
            }
            auto& namedParam = dynamic_cast<NamedTypeInfo&>(*dest);
            if (!namedParam.FundamentalType)
            {
                setError(start, "expected fundamental type for 'long' specifier");
                ptr_ = start;
                return false;
            }
            if (bool const promoted = makeLong(*namedParam.FundamentalType);
                !promoted)
            {
                setError(start, "expected 'int' or 'double' for 'long' specifier");
                ptr_ = start;
                return false;
            }
            if (contains_n(specifiers, "long", 2))
            {
                bool const promoted = makeLong(*namedParam.FundamentalType);
                if (!promoted)
                {
                    setError(start, "expected 'int' or 'double' for 'long long' specifier");
                    ptr_ = start;
                    return false;
                }
            }
            // Add the specifier to the type name
            namedParam.Name->Name = toString(*namedParam.FundamentalType);
        }

        // Final check: if dest is still empty, we have an error
        if (dest.valueless_after_move())
        {
            ptr_ = start;
            setError("expected parameter type");
            return false;
        }

        // Set cv qualifiers
        dest->IsConst = contains(specifiers, "const");
        dest->IsVolatile = contains(specifiers, "volatile");

        return true;
    }

    bool
    parseDeclarationSpecifier(Polymorphic<TypeInfo>& dest)
    {
        char const* start = ptr_;

        // Some rules are only valid if dest was initially empty
        auto checkDestWasEmpty = [destWasEmpty=dest.valueless_after_move(), start, this]() {
            if (!destWasEmpty)
            {
                setError(start, "multiple type declaration specifiers");
                ptr_ = start;
                return false;
            }
            return true;
        };

        // https://en.cppreference.com/w/cpp/language/declarations#Specifiers
        // decl-specifier is one of the following specifiers:
        // - typedef specifier (The typedef specifier may not appear in the declaration of a function parameter)
        // - "inline", "virtual", "explicit" (only allowed in function declarations)
        // - "inline" specifier (also allowed in variable declarations)
        // - "friend" specifier (allowed in class and function declarations)
        // - "constexpr" specifier (allowed in variable and function declarations)
        // - "consteval" specifier (allowed in function declarations)
        // - "constinit" specifier, (allowed in variable declarations)
        // - "register", "static", "extern", "mutable", "thread_local" (storage-class specifiers)
        // - Type specifiers (type-specifier-seq) (a sequence of specifiers that names a type):
        //     - cv qualifier
        if (parseAnyKeyword({"const", "volatile"}))
        {
            return true;
        }

        // - simple type specifier: "char", "char8_t", "char16_t", "char32_t",
        // "wchar_t", "bool", "short", "int", "long", "signed", "unsigned",
        // "float", "double", "void"
        if (parseAnyKeyword({"signed", "unsigned", "short", "long"}))
        {
            // These specifiers are handled in parseDeclarationSpecifiers
            // because they can represent or modify the type.
            return true;
        }

        if (parseAnyKeyword(
                { "char",
                  "char8_t",
                  "char16_t",
                  "char32_t",
                  "wchar_t",
                  "bool",
                  "int",
                  "float",
                  "double",
                  "void" }))
        {
            MRDOCS_CHECK_OR(checkDestWasEmpty(), false);

            dest = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
            auto &NTI = dynamic_cast<NamedTypeInfo &>(*dest);
            MRDOCS_ASSERT(!NTI.Name.valueless_after_move());
            NTI.Name->Name = std::string_view(start, ptr_ - start);
            if (FundamentalTypeKind k;
                fromString(NTI.Name->Name, k))
            {
                NTI.FundamentalType = k;
            }
            return true;
        }

        // - "auto"
        if (parseKeyword("auto"))
        {
            MRDOCS_CHECK_OR(checkDestWasEmpty(), false);
            dest = Polymorphic<TypeInfo>(std::in_place_type<AutoTypeInfo>);
            return true;
        }

        if (parseKeyword("decltype"))
        {
            skipWhitespace();
            //     - "decltype(auto)"
            if (parseLiteral("("))
            {
                skipWhitespace();
                if (parseKeyword("auto"))
                {
                    skipWhitespace();
                    if (parseLiteral(")"))
                    {
                      MRDOCS_CHECK_OR(checkDestWasEmpty(), false);
                      dest = Polymorphic<TypeInfo>(
                          std::in_place_type<AutoTypeInfo>);
                      static_cast<AutoTypeInfo &>(*dest).Keyword =
                          AutoKind::DecltypeAuto;
                      return true;
                    }
                }
                // - "decltype(expression)"
                if (!hasMore())
                {
                    setError("expected expression in decltype");
                    ptr_ = start;
                    return false;
                }
                // rewind ptr_ to '('
                --ptr_;
                while (*ptr_ != '(')
                {
                    ptr_--;
                }
                char const* exprStart = ptr_;
                if (parseBalanced("(", ")"))
                {
                    std::string_view expr(exprStart + 1, ptr_ - exprStart - 2);
                    expr = trim(expr);
                    if (expr.empty())
                    {
                        setError("expected expression in decltype");
                        ptr_ = start;
                        return false;
                    }
                    MRDOCS_CHECK_OR(checkDestWasEmpty(), false);
                    dest = Polymorphic<TypeInfo>(
                        std::in_place_type<DecltypeTypeInfo>);
                    static_cast<DecltypeTypeInfo &>(*dest).Operand.Written =
                        expr;
                    return true;
                }
                setError("expected expression in decltype");
                ptr_ = start;
                return false;
            }
            ptr_ = start;
        }

        // - pack indexing specifier (C++26)
        //   auto f(Ts...[0] arg, type_seq<Ts...>)
        //   (unsupported)

        // - "class" specifier
        // - elaborated type specifier
        //     - "class", "struct" or "union" followed by the identifier
        //        optionally qualified
        //     - "class", "struct" or "union" followed by the template
        //        name with template arguments optionally qualified
        // - typename specifier
        if (parseAnyKeyword({"class", "struct", "union", "typename"}))
        {
            skipWhitespace();
            if (parseQualifiedIdentifier(dest, true, true))
            {
                return checkDestWasEmpty();
            }
            ptr_ = start;
        }

        // - "enum" specifier
        // - "enum" followed by the identifier optionally qualified
        if (parseKeyword("enum"))
        {
            skipWhitespace();
            if (parseQualifiedIdentifier(dest, true, false))
            {
                return checkDestWasEmpty();
            }
            ptr_ = start;
        }

        // - previously declared class/enum/typedef name
        // - template name with template arguments optionally qualified
        // - template name without template arguments optionally qualified
        if (parseQualifiedIdentifier(dest, true, true))
        {
            return checkDestWasEmpty();
        }

        ptr_ = start;
        return false;
    }

    bool
    parseBalanced(
        std::string_view const openTag,
        std::string_view const closeTag,
        std::initializer_list<std::string_view> const until = {})
    {
        char const* start = ptr_;
        std::size_t depth = 0;
        while (hasMore())
        {
            if (depth == 0)
            {
                for (std::string_view const& untilTag : until)
                {
                    if (peek(untilTag))
                    {
                        return true;
                    }
                }
            }
            if (parseLiteral(openTag))
            {
                ++depth;
            }
            else if (parseLiteral(closeTag))
            {
                if (depth == 0)
                {
                    setError("unbalanced expression");
                    ptr_ = start;
                    return false;
                }
                --depth;
                if (depth == 0)
                {
                    return true;
                }
            }
            else
            {
                ++ptr_;
            }
        }
        ptr_ = start;
        return false;
    }

    bool
    parseQualifiedIdentifier(
        Polymorphic<TypeInfo>& dest,
        bool const allowTemplateDisambiguation,
        bool const allowTemplateArguments)
    {
        if (!dest.valueless_after_move())
        {
            setError("type specifier is already set");
            return false;
        }
        // Identifiers separated by "::"
        char const* start = ptr_;
        parseLiteral("::");
        skipWhitespace();
        while (true)
        {
            char const* idStart = ptr_;
            if (!parseIdentifier(allowTemplateDisambiguation))
            {
                break;
            }

            // Populate dest
            auto const idStr = std::string_view(idStart, ptr_ - idStart);
            Optional<Polymorphic<NameInfo>>
                ParentName = !dest.valueless_after_move() ?
                                 Optional<Polymorphic<NameInfo>>(dynamic_cast<NamedTypeInfo*>(&*dest)->Name) :
                                 Optional<Polymorphic<NameInfo>>(std::nullopt);
            dest = Polymorphic<TypeInfo>(std::in_place_type<NamedTypeInfo>);
            auto &NTI = dynamic_cast<NamedTypeInfo &>(*dest);
            NTI.Name = Polymorphic<NameInfo>(std::in_place_type<IdentifierNameInfo>);
            NTI.Name->Name = idStr;
            NTI.Name->Prefix = std::move(ParentName);

            // Look for the next "::"
            char const* compStart = ptr_;
            skipWhitespace();
            if (!parseLiteral("::"))
            {
                ptr_ = compStart;
                break;
            }
            skipWhitespace();
        }
        if (dest.valueless_after_move())
        {
            ptr_ = start;
            return false;
        }
        if (allowTemplateArguments)
        {
            char const* templateStart = ptr_;
            skipWhitespace();
            if (peek('<'))
            {
                if (!dest->isNamed())
                {
                    setError("expected named type for template arguments");
                    ptr_ = start;
                    return false;
                }
                // Replace the nameinfo with a nameinfo with args
                auto& namedParam = dynamic_cast<NamedTypeInfo&>(*dest);
                SpecializationNameInfo SNI;
                SNI.Name = std::move(namedParam.Name->Name);
                SNI.Prefix = std::move(namedParam.Name->Prefix);
                SNI.id = namedParam.Name->id;
                if (!parseTemplateArguments(SNI.TemplateArgs))
                {
                    ptr_ = start;
                    return false;
                }
                namedParam.Name = Polymorphic<NameInfo>(
                    std::in_place_type<SpecializationNameInfo>, std::move(SNI));
            }
            else
            {
                ptr_ = templateStart;
            }
        }
        return true;
    }

    enum declarator_property : int {
        // abstract-declarator - it does not need to be named
        abstract = 1,
        // An internal declarator is any declarator other
        // than a reference declarator (there are no pointers
        // or references to references).
        internal_declarator = 2,
        // noptr-declarator - any valid declarator, but if it begins with *,
        // &, or &&, it has to be surrounded by parentheses.
        no_ptr_declarator = 4,
    };

    bool
    parseAbstractDeclarator(Polymorphic<TypeInfo>& dest)
    {
        return parseDeclarator<abstract>(dest);
    }

    template <int declarator_type>
    bool
    parseDeclarator(Polymorphic<TypeInfo>& dest)
    {
        char const *start = ptr_;
        if (!parseDeclaratorOrNoPtrDeclarator<declarator_type>(dest))
        {
            // Maybe a valid declarator is parenthesized
            if (peek('(', ' '))
            {
                skipWhitespace();
                MRDOCS_ASSERT(parseLiteral('('));
                if (!parseDeclarator<declarator_type>(dest))
                {
                    ptr_ = start;
                    return false;
                }
                skipWhitespace();
                if (!parseLiteral(')'))
                {
                    setError("expected ')'");
                    ptr_ = start;
                    return false;
                }
                return true;
            }
            // We expected a valid declarator either as the
            // complete declarator or as the noptr-declarator
            // for an array or function
            setError("expected declarator");
            ptr_ = start;
            return false;
        }
        if (dest.valueless_after_move())
        {
            setError("no type defined by specifiers and declarator");
            ptr_ = start;
            return false;
        }
        std::size_t suffixLevel = 0;
        auto isNoPtrDeclarator = [&dest, &suffixLevel, this]() {
            if (suffixLevel == 0)
            {
                if (dest->isLValueReference() || dest->isRValueReference() || dest->isPointer())
                {
                    return peekBack(')', ' ');
                }
                // Other types don't need to be surrounded by parentheses
                return true;
            }
            // At other levels, we don't need to check for parentheses
            return true;
        };
        while (
            peekAny({'[', '('}, ' ') &&
            isNoPtrDeclarator())
        {
            // The function return type is the type from the specifiers
            // For instance, in "int (*)", we have a pointer to int.
            // But in "int (*)()", where "int (*)" is the noptr-declarator,
            // the pointer wraps the function type: a pointer to function
            // and not a function to pointer.
            // So parsing "int (*)" gives us a pointer to int (the content
            // of dest), but parsing the function should invert this logic,
            // the pointer points to the function and the function returns int.
            // The same logic other elements that have inner types (pointers,
            // arrays, and references).
            // The current inner type of dest is the function return type.
            auto inner = innerType(*dest);
            // The more suffixes we have, the more levels of inner types
            // the suffix affects.
            // For instance, in "int (*)[3][6]", we have a pointer to an
            // array of 3 arrays of 6 ints.
            std::size_t curSuffixLevel = suffixLevel;
            while (curSuffixLevel > 0 && inner && !inner->valueless_after_move())
            {
                auto& ref = *inner;
                inner = innerType(*ref);
                --curSuffixLevel;
            }
            char const* parenStart = ptr_;
            if (!parseArrayOrFunctionDeclaratorSuffix<declarator_type>(
                inner ? *inner : dest))
            {
                setError(parenStart, "expected declarator");
                ptr_ = start;
                return false;
            }
            ++suffixLevel;
        }
        return true;
    }

    template <int declarator_type>
    bool
    parseDeclaratorOrNoPtrDeclarator(Polymorphic<TypeInfo>& dest)
    {
        static constexpr bool isAbstractDeclarator = static_cast<bool>(declarator_type & abstract);
        static constexpr bool isInternalDeclarator = static_cast<bool>(declarator_type & internal_declarator);

        // https://en.cppreference.com/w/cpp/language/declarations#Declarators
        char const* start = ptr_;

        if (dest.valueless_after_move())
        {
            setError("expected parameter type for '...'");
            ptr_ = start;
            return false;
        }

        // The declarator cannot be another specifier keyword
        // that could also be a declarator
        if (peek(isIdentifierContinuation)
            && parseAnyKeyword(
                { "char",
                  "char8_t",
                  "char16_t",
                  "char32_t",
                  "wchar_t",
                  "bool",
                  "int",
                  "float",
                  "double",
                  "void",
                  "auto",
                  "decltype" }))
        {
            setError("expected declarator, not another specifier");
            ptr_ = start;
            return false;
        }

        // Declarators might be surrounded by an arbitrary
        // number of parentheses. We need to keep track
        // of them.
        skipWhitespace();
        std::size_t parenDepth = 0;
        while (parseLiteral("("))
        {
            ++parenDepth;
            skipWhitespace();
        }

        // At the end, we need to consume the closing parentheses
        // in reverse order.
        auto const parseClosingParens = [&]() {
            while (parenDepth > 0)
            {
                skipWhitespace();
                if (!parseLiteral(")"))
                {
                    setError("expected ')'");
                    ptr_ = start;
                    return false;
                }
                --parenDepth;
            }
            return true;
        };

        // https://en.cppreference.com/w/cpp/language/declarations#Declarators
        // declarator - one of the following:
        // (1) The name that is declared:
        //     unqualified-id attr (optional)
        // https://en.cppreference.com/w/cpp/language/identifiers#Names
        char const* idStart = ptr_;
        if (parseIdentifier(false))
        {
            if (parenDepth != 0 &&
                peek(',', ' '))
            {
                // This is a function parameter declaration
                // and this identifier is actually the type
                // of the first parameter
                ptr_ = idStart;
                MRDOCS_ASSERT(rewindUntil('('));
                parenDepth--;
                if (parseFunctionDeclaratorSuffix<declarator_type>(dest))
                {
                    return parseClosingParens();
                }
            }
            else if (!peek(':', ' '))
            {
                // We ignore the name and just return true
                // The current parameter type does not change
                return parseClosingParens();
            }
            // id is qualified-id, so fall through to the next cases
            ptr_ = idStart;
        }

        // (2) A declarator that uses a qualified identifier (qualified-id)
        //     defines or redeclares a previously declared namespace member
        //     or class member.
        //     qualified-id attr (optional)
        // We do not implement this case for function parameters.

        // (3) Parameter pack, only appears in parameter declarations.
        //     ... identifier attr (optional)
        // https://en.cppreference.com/w/cpp/language/pack
        if (parseLiteral("..."))
        {
            skipWhitespace();
            parseIdentifier(false);
            dest->IsPackExpansion = true;
            return parseClosingParens();
        }

        // (4) Pointer declarator: the declaration `S * D`; declares declarator
        //     `D` as a pointer to the type determined by decl-specifier-seq `S`.
        //     * attr (optional) cv (optional) declarator
        // https://en.cppreference.com/w/cpp/language/pointer
        if (parseLiteral("*"))
        {
            if (dest->isLValueReference() ||
                dest->isRValueReference())
            {
                setError("pointer to reference not allowed");
                ptr_ = start;
                return false;
            }

            // Change current type to pointer type
            PointerTypeInfo PTI;
            PTI.PointeeType = std::move(dest);
            dest = Polymorphic<TypeInfo>(std::move(PTI));

            skipWhitespace();
            // cv is a sequence of const and volatile qualifiers,
            // where either qualifier may appear at most once
            // in the sequence.
            parseCV(dest->IsConst, dest->IsVolatile);
            // Parse the next declarator, potentially wrapping
            // the destination type in another type
            // If this declarator is abstract, the pointer
            // declarator is also abstract.
            if (constexpr int nextDeclaratorType = (declarator_type & abstract)
                                                   | internal_declarator;
                !parseDeclarator<nextDeclaratorType>(dest))
            {
                setError("expected declarator after pointer");
                ptr_ = start;
                return false;
            }
            return parseClosingParens();
        }

        // (5) Pointer to member declaration: the declaration `S C::* D`;
        //     declares `D` as a pointer to member of `C` of type determined
        //     by decl-specifier-seq `S`. nested-name-specifier is a
        //     sequence of names and scope resolution operators ::
        //     nested-name-specifier * attr (optional) cv (optional) declarator
        // https://en.cppreference.com/w/cpp/language/pointer
        if (parseNestedNameSpecifier())
        {
            char const* NNSEnd = ptr_;
            skipWhitespace();
            if (!parseLiteral("*"))
            {
                ptr_ = start;
                return false;
            }

            if constexpr (isInternalDeclarator)
            {
                setError("pointer to member declarator not allowed in this context");
                ptr_ = start;
                return false;
            }

            // Assemble the parent type for the NNS
            NamedTypeInfo ParentType;
            auto NNSString = std::string_view(start, NNSEnd - start);
            IdentifierNameInfo NNS;
            auto const NNSRange = llvm::split(NNSString, "::");
            MRDOCS_ASSERT(!NNSRange.empty());
            auto NNSIt = NNSRange.begin();
            std::string_view unqualID = *NNSIt;
            NNS.Name = std::string(unqualID);
            ++NNSIt;
            while (NNSIt != NNSRange.end())
            {
                unqualID = *NNSIt;
                if (unqualID.empty())
                {
                    break;
                }
                IdentifierNameInfo NewNNS;
                NewNNS.Name = std::string(unqualID);
                NewNNS.Prefix = Polymorphic<NameInfo>(std::move(NNS));
                NNS = NewNNS;
                ++NNSIt;
            }
            ParentType.Name = Polymorphic<NameInfo>(std::move(NNS));

            // Change current type to member pointer type
            MemberPointerTypeInfo MPTI;
            MPTI.PointeeType = Polymorphic<TypeInfo>(std::move(dest));
            MPTI.ParentType = Polymorphic<TypeInfo>(std::move(ParentType));
            dest = Polymorphic<TypeInfo>(std::move(MPTI));

            skipWhitespace();
            // cv is a sequence of const and volatile qualifiers,
            // where either qualifier may appear at most once
            // in the sequence.
            parseCV(dest->IsConst, dest->IsVolatile);
            parseIdentifier(false);
            // We ignore the name and just return true
            return parseClosingParens();
        }

        // (6) Lvalue reference declarator: the declaration `S & D`; declares
        //     `D` as an lvalue reference to the type determined by
        //     decl-specifier-seq `S`.
        //     & attr (optional) declarator
        // https://en.cppreference.com/w/cpp/language/reference
        if (parseLiteral("&"))
        {
            if constexpr (isInternalDeclarator)
            {
                setError("lvalue reference to pointer not allowed");
                ptr_ = start;
                return false;
            }

            skipWhitespace();

            // (7) Rvalue reference declarator: the declaration `S && D`;
            //     declares D as an rvalue reference to the type determined
            //     by decl-specifier-seq `S`.
            //     && attr (optional) declarator

            // Change current type to reference type
            if (bool const isRValue = parseLiteral("&");
                !isRValue)
            {
                LValueReferenceTypeInfo RTI;
                RTI.PointeeType = std::move(dest);
                dest = Polymorphic<TypeInfo>(std::move(RTI));
            }
            else
            {
                RValueReferenceTypeInfo RTI;
                RTI.PointeeType = std::move(dest);
                dest = Polymorphic<TypeInfo>(std::move(RTI));
            }

            skipWhitespace();

            // Parse the next declarator, potentially wrapping
            // the destination type in another type
            static constexpr int nextDeclaratorType
                = (declarator_type & abstract) | internal_declarator;
            if (!parseDeclarator<nextDeclaratorType>(dest))
            {
                setError("expected declarator after reference");
                ptr_ = start;
                return false;
            }

            return parseClosingParens();
        }

        // (8-9) Array and function declarators are handled in a separate
        // function
        parenDepth = 0;
        ptr_ = start;
        if (parseArrayOrFunctionDeclaratorSuffix<declarator_type>(dest))
        {
            return true;
        }

        // (10) An abstract declarator can also be an empty string, which
        // is equivalent to unnamed (1) unqualified-id.
        if constexpr (isAbstractDeclarator)
        {
            return parseClosingParens();
        }
        else
        {
            setError("expected a concrete declarator");
            ptr_ = start;
            return false;
        }
    }

    // This function assumes the noptr-declarator prefix
    // already parsed. Otherwise, we assume the noptr-declarator
    // is empty.
    template <int declarator_type>
    bool
    parseArrayOrFunctionDeclaratorSuffix(Polymorphic<TypeInfo>& dest)
    {
        char const* start = ptr_;

        // (8) Array declarator. noptr-declarator any valid declarator, but
        //     if it begins with *, &, or &&, it has to be surrounded by
        //     parentheses.
        // noptr-declarator [expr (optional)] attr (optional)
        // https://en.cppreference.com/w/cpp/language/array
        if (parseArrayDeclaratorSuffix<declarator_type>(dest))
        {
            return true;
        }
        ptr_ = start;

        // (9) Function declarator. noptr-declarator any valid declarator,
        //     but if it begins with *, &, or &&, it has to be surrounded by
        //     parentheses. It may end with the optional trailing return type.
        //     noptr-declarator ( parameter-list ) cv (optional) ref  (optional) except (optional) attr  (optional)
        // https://en.cppreference.com/w/cpp/language/function
        // https://en.cppreference.com/w/cpp/language/function#Function_type
        if (parseFunctionDeclaratorSuffix<declarator_type>(dest))
        {
            return true;
        }

        return false;
    }

    template <int declarator_type>
    bool
    parseArrayDeclaratorSuffix(Polymorphic<TypeInfo>& dest)
    {
        char const* start = ptr_;

        // (8) Array declarator. noptr-declarator any valid declarator, but
        //     if it begins with *, &, or &&, it has to be surrounded by
        //     parentheses.
        // noptr-declarator [expr (optional)] attr (optional)
        // https://en.cppreference.com/w/cpp/language/array
        if (parseLiteral("["))
        {
            if constexpr (declarator_type & internal_declarator)
            {
                setError("pointer to array declarator requires noptr-declarator");
                ptr_ = start;
                return false;
            }

            // Change current type to array type
            ArrayTypeInfo ATI;
            ATI.ElementType = std::move(dest);

            // expr (optional)
            char const* exprStart = ptr_;
            skipWhitespace();

            if (!parseLiteral("]"))
            {
                // Parse the array size
                // Bounds.Value is an optional integer with the value
                // Bounds.Written is the original string representation
                // of the bounds
                Optional<std::uint64_t> boundsValue = 0;
                if (ConstantExprInfo<std::uint64_t> Bounds;
                    parseInteger(boundsValue) &&
                    peek(']', ' '))
                {
                    Bounds.Value = *boundsValue;
                    Bounds.Written = std::string_view(exprStart, ptr_ - exprStart);
                    ATI.Bounds = Bounds;
                    if (!parseLiteral("]"))
                    {
                        ptr_ = start;
                        return false;
                    }
                }
                else
                {
                    ptr_ = start;
                    // Parse everything up to the next
                    // closing bracket
                    if (!parseBalanced("[", "]"))
                    {
                        setError("expected ']' in array declarator");
                        ptr_ = start;
                        return false;
                    }
                    auto expr = std::string_view(exprStart, ptr_ - exprStart - 1);
                    Bounds.Written = trim(expr);
                    ATI.Bounds = Bounds;
                }
            }
            dest = Polymorphic<TypeInfo>(std::move(ATI));
            skipWhitespace();

            // We ignore the name and just return true
            return true;
        }
        return false;
    }

    template <int declarator_type>
    bool
    parseFunctionDeclaratorSuffix(Polymorphic<TypeInfo>& dest)
    {
        char const* start = ptr_;

        // (9) Function declarator. noptr-declarator any valid declarator,
        //     but if it begins with *, &, or &&, it has to be surrounded by
        //     parentheses. It may end with the optional trailing return type.
        //     noptr-declarator ( parameter-list ) cv (optional) ref  (optional) except (optional) attr  (optional)
        // https://en.cppreference.com/w/cpp/language/function
        // https://en.cppreference.com/w/cpp/language/function#Function_type
        if (peek('(', ' '))
        {
            if constexpr (declarator_type & internal_declarator)
            {
                setError("pointer to function declarator requires noptr-declarator");
                ptr_ = start;
                return false;
            }

            // Change current type to function type
            // The function type as a parameter has the following members:
            // FTI.ReturnType is the return type of the function
            // FTI.ParamTypes is a list of parameter types
            // FTI.RefQualifier is the reference qualifier
            // FTI.ExceptionSpec is the exception specification
            // FTI.IsVariadic is true if the function is variadic
            // Parse the function parameters
            ParsedFunctionSuffix function;
            if (!parseFunctionSuffix(function))
            {
                ptr_ = start;
                return false;
            }
            FunctionTypeInfo FTI;
            FTI.ReturnType = std::move(dest);
            FTI.ParamTypes.insert(
                FTI.ParamTypes.end(),
                std::make_move_iterator(function.Params.begin()),
                std::make_move_iterator(function.Params.end()));
            FTI.ExceptionSpec = std::move(function.ExceptionSpec);
            FTI.IsVariadic = function.IsVariadic;
            dest = Polymorphic<TypeInfo>(std::move(FTI));
            return true;
        }

        return false;
    }

    bool
    parseInteger(Optional<std::uint64_t>& dest)
    {
        if (!hasMore())
        {
            return false;
        }
        if (!isDigit(*ptr_))
        {
            return false;
        }
        std::uint64_t value = 0;
        while (isDigit(*ptr_))
        {
            value = value * 10 + (*ptr_ - '0');
            ++ptr_;
        }
        dest = value;
        return true;
    }

    bool
    parseCV(bool& isConst, bool& isVolatile) {
        char const* start = ptr_;
        while (true)
        {
            skipWhitespace();
            bool matchedAny = false;
            if (parseKeyword("const"))
            {
                if (isConst)
                {
                    setError("multiple 'const' qualifiers");
                    ptr_ = start;
                    return false;
                }
                isConst = true;
                matchedAny = true;
            }
            if (parseKeyword("volatile"))
            {
                if (isVolatile)
                {
                    setError("multiple 'volatile' qualifiers");
                    ptr_ = start;
                    return false;
                }
                isVolatile = true;
                matchedAny = true;
            }
            if (!matchedAny)
            {
                break;
            }
        }
        return true;
    }

    bool
    parseNestedNameSpecifier()
    {
        // nested-name-specifier is a sequence of names and
        // scope resolution operators ::
        char const* start = ptr_;
        parseLiteral("::");
        bool hasAnyIdentifier = false;
        while (true)
        {
            if (parseIdentifier(false))
            {
                hasAnyIdentifier = true;
            }
            else
            {
                if (hasAnyIdentifier)
                {
                    return true;
                }
                ptr_ = start;
                return false;
            }
            skipWhitespace();
            if (!parseLiteral("::"))
            {
                setError("expected '::' in nested name specifier");
                ptr_ = start;
                return false;
            }
            skipWhitespace();
        }
    }

    bool
    parseFunctionQualifiers(ParsedFunctionSuffix& dest)
    {
        // https://en.cppreference.com/w/cpp/language/function
        char const* start = ptr_;

        auto* destMF = dynamic_cast<ParsedMemberFunctionSuffix*>(&dest);
        if (destMF && !destMF->IsExplicitObjectMemberFunction)
        {
            // Parse cv:
            // const/volatile qualification, only allowed in non-static member
            // function declarations
            if (!parseCV(destMF->IsConst, destMF->IsVolatile))
            {
                setError("expected cv qualifiers");
                ptr_ = start;
                return false;
            }
        }

        // Parse ref:
        // ref-qualification, only allowed in non-static member function
        // declarations
        if (destMF && !destMF->IsExplicitObjectMemberFunction)
        {
            skipWhitespace();
            if (parseLiteral("&"))
            {
                destMF->Kind = ReferenceKind::LValue;
                skipWhitespace();
                if (parseLiteral("&"))
                {
                    destMF->Kind = ReferenceKind::RValue;
                    skipWhitespace();
                }
            }
        }

        // Parse except:
        // dynamic exception specification, dynamic exception specification
        // or noexcept specification, noexcept specification
        // https://en.cppreference.com/w/cpp/language/noexcept_spec
        if (parseKeyword("noexcept"))
        {
            dest.ExceptionSpec.Implicit = false;
            skipWhitespace();
            char const *noexceptStart = ptr_;
            if (parseBalanced("(", ")"))
            {
                char const* noexceptEnd = ptr_;
                std::string_view const expression(
                    noexceptStart + 1,
                    noexceptEnd - noexceptStart - 2);
                dest.ExceptionSpec.Operand = trim(expression);
                dest.ExceptionSpec.Kind =
                    dest.ExceptionSpec.Operand == "true" ?
                        NoexceptKind::True :
                    dest.ExceptionSpec.Operand == "false" ?
                        NoexceptKind::False :
                        NoexceptKind::Dependent;
            }
        }
        else if (parseKeyword("throw"))
        {
            skipWhitespace();
            if (!parseLiteral("("))
            {
                setError("expected '(' in 'throw' exception specification");
                ptr_ = start;
                return false;
            }
            skipWhitespace();
            if (!parseLiteral(")"))
            {
                setError("expected ')' for empty 'throw' exception specification");
                ptr_ = start;
                return false;
            }
            dest.ExceptionSpec.Implicit = false;
            dest.ExceptionSpec.Operand = "true";
            dest.ExceptionSpec.Kind = NoexceptKind::True;
        }

        return true;
    }

    constexpr
    bool
    hasMore() const noexcept
    {
        return ptr_ != last_;
    }

    bool
    skipWhitespace()
    {
        if (!hasMore() || !std::isspace(*ptr_))
        {
            return false;
        }
        while (hasMore() && std::isspace(*ptr_))
        {
            ++ptr_;
        }
        return true;
    }
};

} // (anon)

// Function to parse a C++ symbol name
ParseResult
parse(
    char const* first,
    char const* last,
    ParsedRef& value)
{
    RefParser parser(first, last, value);
    ParseResult res;
    if (parser.parse())
    {
        res.ptr = parser.position();
    }
    else
    {
        res.ec = parser.error();
        res.ptr = parser.errorPos();
    }
    return res;
}

} // clang::mrdocs
