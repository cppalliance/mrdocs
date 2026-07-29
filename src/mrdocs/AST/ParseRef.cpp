//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ParseRef.hpp"
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/String/String.hpp>
#include <llvm/ADT/StringExtras.h>
#include <format>

namespace mrdocs {

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
    llvm::SmallVector<Polymorphic<Type>, 8> Params;

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
        Optional<Polymorphic<Type>> conversionType = std::nullopt;
        if (!parseDeclarationSpecifier(conversionType) ||
            !conversionType)
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
        Optional<Polymorphic<Type>> type = std::nullopt;
        if (parseTypeId(type))
        {
            MRDOCS_ASSERT(type);
            dest = Polymorphic<TArg>(std::in_place_type<TypeTArg>);
            dest->asType().Type = std::move(*type);
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
        dest = Polymorphic<TArg>(std::in_place_type<ConstantTArg>);
        static_cast<ConstantTArg&>(*dest).Value.Written =
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
        Optional<Polymorphic<Type>> curParamOpt = std::nullopt;
        if (!parseTypeId(curParamOpt))
        {
            ptr_ = start;
            setError("expected type-id");
            return false;
        }
        MRDOCS_ASSERT(curParamOpt);
        auto &curParam = dest.Params.emplace_back(std::move(*curParamOpt));

        // 2. After determining the type of each parameter, any parameter
        // of type “array of T” or of function type T is adjusted to be
        // “pointer to T”.
        // https://en.cppreference.com/w/cpp/language/function#Function_type
        if (curParam->isArray())
        {
            ArrayType PrevParamType = curParam->asArray();
            curParam = Polymorphic<Type>(std::in_place_type<PointerType>);
            auto& curAsPointerType = curParam->asPointer();
            curAsPointerType.PointeeType = std::move(PrevParamType.ElementType);
            auto& PrevAsBase = PrevParamType.asType();
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
    parseTypeId(Optional<Polymorphic<Type>>& dest)
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

#include "ParseRef/Declarators.ipp"


    // This function assumes the noptr-declarator prefix
    // already parsed. Otherwise, we assume the noptr-declarator
    // is empty.
    template <int declarator_type>
    bool
    parseArrayOrFunctionDeclaratorSuffix(Polymorphic<Type>& dest)
    {
        MRDOCS_ASSERT(!dest.valueless_after_move());

        char const* start = ptr_;

        // (8) Array declarator. noptr-declarator any valid declarator, but
        //     if it begins with *, &, or &&, it has to be surrounded by
        //     parentheses.
        // noptr-declarator [expr (optional)] attr (optional)
        // https://en.cppreference.com/w/cpp/language/array
        Optional<Polymorphic<Type>> r;
        if (parseArrayDeclaratorSuffix<declarator_type>(r))
        {
            dest = *r;
            return true;
        }
        ptr_ = start;

        // (9) Function declarator. noptr-declarator any valid declarator,
        //     but if it begins with *, &, or &&, it has to be surrounded by
        //     parentheses. It may end with the optional trailing return type.
        //     noptr-declarator ( parameter-list ) cv (optional) ref  (optional) except (optional) attr  (optional)
        // https://en.cppreference.com/w/cpp/language/function
        // https://en.cppreference.com/w/cpp/language/function#Function_type
        if (parseFunctionDeclaratorSuffix<declarator_type>(r))
        {
            dest = *r;
            return true;
        }

        return false;
    }

    template <int declarator_type>
    bool
    parseArrayDeclaratorSuffix(Optional<Polymorphic<Type>>& dest)
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
            ArrayType ATI;
            if (dest)
            {
                ATI.ElementType = std::move(*dest);
            }

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
            dest = Polymorphic<Type>(std::move(ATI));
            skipWhitespace();

            // We ignore the name and just return true
            return true;
        }
        return false;
    }

    template <int declarator_type>
    bool
    parseFunctionDeclaratorSuffix(Optional<Polymorphic<Type>>& dest)
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
            FunctionType FTI;
            if (dest)
            {
                FTI.ReturnType = std::move(*dest);
            }
            FTI.ParamTypes.insert(
                FTI.ParamTypes.end(),
                std::make_move_iterator(function.Params.begin()),
                std::make_move_iterator(function.Params.end()));
            FTI.ExceptionSpec = std::move(function.ExceptionSpec);
            FTI.IsVariadic = function.IsVariadic;
            dest = Polymorphic<Type>(std::move(FTI));
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

} // mrdocs
