//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/AST/ParseRef.hpp"
#include "mrdocs/Metadata/Info/Function.hpp"

namespace clang::mrdocs {

namespace {
constexpr
bool
isDigit(char c)
{
    return c >= '0' && c <= '9';
}

constexpr
bool
isIdentifierStart(char c)
{
    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_' || c == '~';
}

constexpr
bool
isIdentifierContinuation(char c)
{
    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_' || isDigit(c);
}

class RefParser
{
    char const* first_;
    char const* ptr_;
    char const* last_;
    ParsedRef result_;
    std::string error_;
    char const* error_pos_{nullptr};

public:
    explicit
    RefParser(std::string_view const str)
        : first_(str.data())
        , ptr_(first_)
        , last_(first_ + str.size())
    {
    }

    bool
    parse()
    {
        skipWhitespace();
        if (parseLiteral("::"))
        {
            result_.IsFullyQualified = true;
        }
        MRDOCS_CHECK_OR(parseComponents(), false);
        skipWhitespace();
        if (peek('('))
        {
            MRDOCS_CHECK_OR(parseFunctionParameters(), false);
            parseMemberFunctionQualifiers();
        }
        return true;
    }

    Expected<ParsedRef>
    result() const
    {
        if (error_pos_ == nullptr)
        {
            return result_;
        }
        std::string_view str(first_, last_ - first_);
        std::size_t pos = error_pos_ - first_;
        return Unexpected(formatError("'{}' at position {}: {}", str, pos, error_));
    }

private:
    void
    setError(std::string_view str)
    {
        // Only set the error if it's not already set
        // with a more specified error message
        if (error_.empty())
        {
            error_ = std::string(str);
            error_pos_ = ptr_;
        }
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

    bool
    parseComponents()
    {
        while (parseComponent())
        {
            skipWhitespace();
            if (!parseLiteral("::"))
            {
                break;
            }
        }
        return !result_.Components.empty();
    }

    bool
    parseComponent()
    {
        if (!hasMore())
        {
            return false;
        }
        char const *start = ptr_;
        if (skipWhitespace())
        {
            setError("unexpected whitespace");
            return false;
        }
        result_.Components.emplace_back();
        if (!parseComponentName())
        {
            setError("expected component name");
            ptr_ = start;
            return false;
        }
        skipWhitespace();
        if (peek('<')) {
            if (!parseTemplateArguments())
            {
                setError("expected template arguments");
                ptr_ = start;
                return false;
            }
        }

        return true;
    }

    bool
    parseComponentName()
    {
        char const* start = ptr_;

        if (!hasMore())
        {
            setError("expected component name");
            return false;
        }

        // Try to parse as an operator
        if (parseOperator())
        {
            return true;
        }

        // Parse as a regular identifier
        if (!parseIdentifier())
        {
            setError("expected component name");
            ptr_ = start;
            return false;
        }
        currentComponent().Name = std::string_view(start, ptr_ - start);
        currentComponent().Operator = OperatorKind::None;
        return true;
    }

    bool
    parseIdentifier()
    {
        char const* start = ptr_;
        skipWhitespace();
        if (!hasMore())
        {
            setError("expected identifier");
            ptr_ = start;
            return false;
        }
        if (!isIdentifierStart(*ptr_))
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

    ParsedRefComponent&
    currentComponent()
    {
        return result_.Components.back();
    }

    bool
    parseOperator()
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
                currentComponent().Operator = getOperatorKindFromSuffix(op);
                MRDOCS_ASSERT(currentComponent().Operator != OperatorKind::None);
                currentComponent().Name = getOperatorName(currentComponent().Operator, true);
                return true;
            }
        }

        // Handle other operator types by looking at the first
        // character equal to "<(.:"
        char const* op_start = ptr_;
        while (hasMore())
        {
            if (*ptr_ == '<' || *ptr_ == '(' || *ptr_ == '.' || *ptr_ == ':')
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
        currentComponent().Operator = getOperatorKindFromSuffix(op);
        if (currentComponent().Operator == OperatorKind::None)
        {
            // This operator doesn't exist
            ptr_ = start;
            return false;
        }
        currentComponent().Name = getOperatorName(currentComponent().Operator, true);
        return true;
    }

    bool
    parseTemplateArguments()
    {
        char const* start = ptr_;
        if (!parseLiteral('<'))
        {
            ptr_ = start;
            return false;
        }
        skipWhitespace();
        while (parseTemplateArgument())
        {
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
        if (!parseLiteral('>'))
        {
            setError("expected '>'");
            ptr_ = start;
            return false;
        }
        return true;
    }

    bool
    parseTemplateArgument()
    {
        if (!hasMore())
        {
            return false;
        }
        skipWhitespace();
        char const* start = ptr_;
        currentComponent().TemplateArguments.emplace_back();
        if (!parseTemplateArgumentName())
        {
            ptr_ = start;
            setError("expected template argument name");
            return false;
        }
        skipWhitespace();
        return true;
    }

    std::string_view&
    currentTemplateArgument()
    {
        return currentComponent().TemplateArguments.back();
    }

    bool
    parseTemplateArgumentName()
    {
        char const* start = ptr_;

        if (!hasMore())
        {
            setError("expected component name");
            return false;
        }

        // Parse as a regular identifier
        if (!parseIdentifier())
        {
            setError("expected identifier name");
            ptr_ = start;
            return false;
        }
        currentTemplateArgument() = std::string_view(start, ptr_ - start);
        return true;
    }

    bool
    parseFunctionParameters()
    {
        char const* start = ptr_;
        if (!parseLiteral('('))
        {
            ptr_ = start;
            return false;
        }
        skipWhitespace();
        while (parseFunctionParameter())
        {
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
        return true;
    }

    bool
    parseFunctionParameter()
    {
        if (!hasMore())
        {
            return false;
        }
        skipWhitespace();
        char const* start = ptr_;
        result_.FunctionParameters.emplace_back();
        if (!parseFunctionParameterName())
        {
            ptr_ = start;
            setError("expected function parameter");
            return false;
        }
        skipWhitespace();
        return true;
    }

    std::string_view&
    currentFunctionParameter()
    {
        return result_.FunctionParameters.back();
    }

    bool
    parseFunctionParameterName()
    {
        char const* start = ptr_;
        if (!hasMore())
        {
            setError("expected parameter name");
            return false;
        }
        skipWhitespace();

        // Empty parameter (MrDocs will fall back to a function with the same
        // number of parameters)
        if (parseLiteral(","))
        {
            currentFunctionParameter() = {};
            return true;
        }

        // Variadic parameter
        if (parseLiteral("..."))
        {
            currentFunctionParameter() = trim(std::string_view(start, ptr_ - start));
            return true;
        }

        // Parse leading qualifiers
        while (parseLiteral("const") || parseLiteral("volatile"))
        {
            skipWhitespace();
        }

        // Parse as a type, which is a list of regular identifiers
        if (!parseTypeName())
        {
            setError("expected type name");
            ptr_ = start;
            return false;
        }

        currentFunctionParameter() = std::string_view(start, ptr_ - start);
        return true;
    }

    bool
    parseTypeName()
    {
        // A list of identifiers separated by "::"
        char const* start = ptr_;
        if (!(parseModifiedFundamentalType() || parseIdentifier()))
        {
            return false;
        }
        while (parseLiteral("::"))
        {
            if (!parseIdentifier())
            {
                ptr_ = start;
                return false;
            }
        }
        return true;
    }

    bool
    parseModifiedFundamentalType()
    {
        char const* start = ptr_;
        bool hasSignModifier = false;
        bool hasSizeModifier = false;
        bool hasFundamentalType = false;
        while (true)
        {
            skipWhitespace();
            if (!hasSignModifier)
            {
                if (parseLiteral("signed") || parseLiteral("unsigned"))
                {
                    hasSignModifier = true;
                    continue;
                }
            }
            if (parseLiteral("short") || parseLiteral("long"))
            {
                hasSizeModifier = true;
                continue;
            }
            if (!hasFundamentalType)
            {
                if (parseLiteral("int") || parseLiteral("char") || parseLiteral("float") || parseLiteral("double"))
                {
                    hasFundamentalType = true;
                    continue;
                }
            }
            break;
        }
        if (!hasFundamentalType && !hasSizeModifier && !hasSignModifier)
        {
            ptr_ = start;
            return false;
        }
        // Any of the other combinations are valid
        return true;
    }

    bool
    parseMemberFunctionQualifiers()
    {
        // Parse potential qualifiers at the end of a function
        // to identify the qualifiers to set
        // ReferenceKind Kind = ReferenceKind::None; and
        // bool IsConst = false;
        skipWhitespace();
        if (parseLiteral("const"))
        {
            result_.IsConst = true;
        }
        skipWhitespace();
        if (parseLiteral("&&"))
        {
            result_.Kind = ReferenceKind::RValue;
        }
        else if (parseLiteral("&"))
        {
            result_.Kind = ReferenceKind::LValue;
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
Expected<ParsedRef>
parseRef(std::string_view sv)
{
    RefParser parser(sv);
    parser.parse();
    return parser.result();
}

} // clang::mrdocs
