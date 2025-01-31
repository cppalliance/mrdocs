//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "NameParser.hpp"
#include <functional>

namespace clang::mrdocs {

namespace {

class TokenStream
{
    char const* first_;
    char const* part_;
    char const* ptr_;
    char const* last_;

public:
    TokenStream(std::string_view str)
        : first_(str.data())
        , part_(first_)
        , ptr_(first_)
        , last_(first_ + str.size())
    {
    }

    void discardPart()
    {
        part_ = ptr_;
    }

    void appendPart(std::string& part)
    {
        part.append(part_, ptr_);
        discardPart();
    }

    std::size_t remaining() const noexcept
    {
        return last_ - ptr_;
    }

    std::size_t consumed() const noexcept
    {
        return ptr_ - first_;
    }

    bool valid() const noexcept
    {
        return ptr_ != last_;
    }

    explicit operator bool() const noexcept
    {
        return valid();
    }

    char operator*() const noexcept
    {
        MRDOCS_ASSERT(valid());
        return *ptr_;
    }

    bool consume(std::size_t n = 1)
    {
        MRDOCS_ASSERT(remaining() >= n);
        ptr_ += n;
        return valid();
    }

    TokenStream& operator++() noexcept
    {
        consume();
        return *this;
    }

    bool tryConsume(std::string_view str)
    {
        std::string_view data(ptr_, last_);
        if(! data.starts_with(str))
            return false;
        consume(str.size());
        return true;
    }

    template<typename Predicate>
    std::string_view consumeWhile(Predicate&& pred)
    {
        const auto first = ptr_;
        while(valid() && pred(**this))
            consume();
        return std::string_view(first, ptr_);
    }

    template<typename Predicate>
    std::string_view consumeUntil(Predicate&& pred)
    {
        return consumeWhile(std::not_fn(
            std::forward<Predicate>(pred)));
    }
};

// whether a character is a digit, per the C++ grammar
template<bool AllowWildcards>
bool isDigit(char c)
{
    return (AllowWildcards && c == '*') ||
        (c >= '0' && c <= '9');
}

// whether a character is a non-digit, per the C++ grammar
template<bool AllowWildcards>
bool isNonDigit(char c)
{
    if((AllowWildcards && c == '*') || c == '_')
        return true;
    c |= 0x20;
    return c >= 'a' && c <= 'z';
}

template<bool AllowWildcards>
class IdExpressionParser
{
    TokenStream s_;
    ParseResult& result_;

    enum class IdentifierKind
    {
        Normal,
        Typename,
        Template,
        Operator,
        Decltype
    };

    static bool isIdentifierStart(char c)
    {
        return c == '~' || isNonDigit<AllowWildcards>(c);
    }

    static bool isIdentifierContinue(char c)
    {
        return c != '~' && (isNonDigit<AllowWildcards>(c) ||
            isDigit<AllowWildcards>(c));
    }

    template<typename... Args>
    void
    fail(
        std::string_view err_msg,
        Args&&... err_args)
    {
        formatError(err_msg, std::forward<Args>(err_args)...).Throw();
    }

    void commit()
    {
        s_.appendPart(result_.name);
    }

    void discard()
    {
        s_.discardPart();
    }

    bool skipWhitespace()
    {
        s_.consumeWhile([](char c)
        {
            // we consider carriage returns to be whitespace
            return c == ' ' || c == '\n' || c == '\t' ||
                c == '\v' || c == '\f' || c == '\r';
        });
        discard();
        return ! s_;
    }

    template<typename... Args>
    void
    skipWhitespace(
        std::string_view err_msg,
        Args&&... err_args)
    {
        if(skipWhitespace())
            fail(err_msg, std::forward<Args>(err_args)...);
    }

    void
    skipBalanced(
        char start_tok,
        char end_tok)
    {
        MRDOCS_ASSERT(*s_ == start_tok);
        // skip opening token
        ++s_;
        while(true)
        {
            s_.consumeWhile([&](char c)
            {
                return c != start_tok && c != end_tok;
            });
            if(! s_)
                fail("expected '{}' to match '{}'", end_tok, start_tok);
            // found end token, done matching at this level
            if(*s_ == end_tok)
                break;
            skipBalanced(start_tok, end_tok);
        }
        // skip closing token
        ++s_;
    }

    void parseColonColon()
    {
        MRDOCS_ASSERT(*s_ == ':');
        if(! ++s_ || *s_ != ':')
            fail("expected ':' after ':'");
        ++s_;
        skipWhitespace("expected unqualified-id after '::'");
    }

    IdentifierKind parseIdentifier()
    {
        MRDOCS_ASSERT(isIdentifierStart(*s_));
        if(! isIdentifierStart(*s_))
            fail("expected identifier-start");
        auto id = s_.consumeWhile(isIdentifierContinue);
        if(id == "operator")
            return IdentifierKind::Operator;
        if(id == "template")
            return IdentifierKind::Template;
        if(id == "typename")
            return IdentifierKind::Typename;
        if(id == "decltype")
            return IdentifierKind::Decltype;
        return IdentifierKind::Normal;
    }

    void parseOperator()
    {
        switch(char first = *s_)
        {
        case ',':
        case '~':
        {
            ++s_;
            break;
        }
        case '(':
        case '[':
        {
            ++s_;
            commit();
            char close = first == '(' ? ')' : ']';
            if(skipWhitespace() || *s_ != close)
                fail("expected '{}' after 'operator {}'", close, first);
            ++s_;
            break;
        }
        case '*':
        case '%':
        case '/':
        case '^':
        case '=':
        case '!':
        {
            if(! ++s_ || *s_ != '=')
                break;
            ++s_;
            break;
        }
        case '+':
        case '|':
        case '&':
        {
            if(! ++s_ || (*s_ != first && *s_ != '='))
                break;
            ++s_;
            break;
        }
        case '>':
        {
            if(! ++s_)
                break;

            if(*s_ == '>')
                ++s_;

            if(s_ && *s_ == '=')
                ++s_;
            break;
        }
        case '<':
        {
            if(! ++s_)
                break;

            bool is_shift = *s_ == '<';
            if(is_shift)
                ++s_;

            if(s_ && *s_ == '=')
            {
                if(++s_ && ! is_shift && *s_ == '>')
                    ++s_;
            }
            break;
        }
        case '-':
        {
            if(! ++s_)
                break;
            if(*s_ == '-' || *s_ == '=')
                ++s_;
            else if(*s_ == '>' && ++s_ && *s_ == '*')
                ++s_;
            break;
        }
        case 'c':
        {
            if(! s_.tryConsume("co_await"))
                fail("invalid operator name");
            break;
        }
        case 'n':
        case 'd':
        {
            std::string_view name =
                first == 'n' ? "new" : "delete";
            if(! s_.tryConsume(name))
                fail("invalid operator name");
            commit();
            // consume [], if present
            if(! skipWhitespace() && *s_ == '[')
            {
                commit();
                if(skipWhitespace() || *s_ != ']')
                    fail("expected ']' after 'operator {}['", name);
                ++s_;
            }
            break;
        }
        default:
            fail("invalid operator name");
            break;
        }
        commit();
    }

    bool parseName()
    {
        switch(parseIdentifier())
        {
        // simple identifier
        case IdentifierKind::Normal:
            // store the identifier
            commit();

            // parse the template argument list, if any
            if(! skipWhitespace() && *s_ == '<')
            {
                // parse the template argument list
                skipBalanced('<', '>');
                commit();
            }
            return false;
        // operator name or conversion-function-id
        case IdentifierKind::Operator:
            // store 'operator'
            commit();
            skipWhitespace("expected operator name");
            // parse the tokens after 'operator'.
            // KRYSTIAN TODO: support conversion functions
            parseOperator();

            // parse the template argument list, if any
            if(! skipWhitespace() && *s_ == '<')
            {
                // parse the template argument list
                skipBalanced('<', '>');
                commit();
            }
            // operator names are always terminal
            return true;
        // 'template' followed by simple-template-id
        case IdentifierKind::Template:
            // KRYSTIAN FIXME: do we want to store this?
            // discard 'template'
            discard();

            skipWhitespace("expected template-id after 'template'");

            // KRYSTIAN FIXME: we should restrict the names permitted
            // to come after 'template' here
            return parseName();
        // typename-specifier
        case IdentifierKind::Typename:
            // KRYSTIAN FIXME: do we want to store this?
            // discard 'typename'
            discard();

            skipWhitespace("expected nested-name-specifier after 'typename'");

            // KRYSTIAN FIXME: we should restrict the names permitted
            // to come after 'typename' here
            return parseName();
        // decltype-specifier
        case IdentifierKind::Decltype:
            // store 'decltype'
            commit();

            if(skipWhitespace() || *s_ != '(')
                fail("expected '(' after 'decltype'");

            // parse the operand of the decltype-specifier
            skipBalanced('(', ')');
            commit();
            return false;
        default:
            MRDOCS_UNREACHABLE();
        }
    }

public:
    IdExpressionParser(
        std::string_view str,
        ParseResult& result)
        : s_(str)
        , result_(result)
    {
    }

    void parse()
    {
        skipWhitespace("expected id-expression");

        // qualified-id starting with '::'
        if(*s_ == ':')
        {
            result_.qualified = true;
            parseColonColon();
        }

        // now parse the optional nested-name-specifier,
        // followed by an unqualified-id
        while(! parseName())
        {
            // if we are out of characters, or if we have a possibly
            // invalid character following the name (which could be
            // the parameter types), stop parsing
            if(skipWhitespace() || *s_ != ':')
                break;

            result_.qualified = true;
            result_.qualifier.emplace_back(
                std::move(result_.name));
            result_.name.clear();

            parseColonColon();
        }

        // parse parameter types
        if(s_ && *s_ == '(')
        {
            discard();
            // KRYSTIAN FIXME: right now we drop the parameters
            skipBalanced('(', ')');
        }
    }
};

} // (anon)

Expected<ParseResult>
parseIdExpression(
    std::string_view const str,
    bool const allow_wildcards)
{
    try
    {
        ParseResult result;
        if (allow_wildcards)
        {
            IdExpressionParser<true>(str, result).parse();
        } else
        {
            IdExpressionParser<false>(str, result).parse();
        }
        return std::move(result);
    }
    catch(Exception const& ex)
    {
        return Unexpected(ex.error());
    }
}

} // clang::mrdocs
