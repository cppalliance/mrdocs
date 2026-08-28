//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/TagfileReader.hpp>
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/String/String.hpp>
#include <llvm/Support/ConvertUTF.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

// What a tagfile is made of, once the punctuation is out of the way.
enum class TokenKind
{
    Open,   // `<name ...>`
    Close,  // `</name>`
    Empty,  // `<name .../>`
    Text,   // between tags
    End     // nothing left
};

struct Token
{
    TokenKind kind = TokenKind::End;
    std::string name;
    std::string kindAttribute;
    std::string text;
};

struct Attribute
{
    std::string name;
    std::string value;
};

bool
isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool
isNameStart(char c)
{
    return isAlphabetic(c) || c == '_' || c == ':';
}

bool
isNameChar(char c)
{
    return isNameStart(c) || isDigit(c) || c == '-' || c == '.';
}

// What a named reference stands for, if it is one XML predefines.
std::optional<char>
namedEntity(std::string_view body)
{
    using Entity = std::pair<std::string_view, char>;
    static constexpr Entity predefined[] = {
        {"lt", '<'},
        {"gt", '>'},
        {"amp", '&'},
        {"quot", '"'},
        {"apos", '\''}
    };
    Entity const* const entry =
        std::ranges::find(predefined, body, &Entity::first);
    return entry != std::ranges::end(predefined)
        ? std::optional<char>(entry->second)
        : std::nullopt;
}

/*  The UTF-8 text a character reference stands for, or nothing if it
    names no character: the digits may be missing, or not digits, or
    name a surrogate or a code point past the end of Unicode.

    @param digits What follows the `#`, read as hexadecimal if it starts
        with an `x` and as decimal otherwise.
*/
std::optional<std::string>
referencedCharacter(std::string_view digits)
{
    std::optional<std::string> result;
    bool const hex = digits.starts_with("x") || digits.starts_with("X");
    std::string_view const number = hex ? digits.substr(1) : digits;
    unsigned code = 0;
    std::array<char, UNI_MAX_UTF8_BYTES_PER_CODE_POINT> buffer{};
    char* end = buffer.data();
    if (!number.empty()
        && std::from_chars(
               number.data(),
               number.data() + number.size(),
               code,
               hex ? 16 : 10).ec == std::errc{}
        && llvm::ConvertCodePointToUTF8(code, end))
    {
        result.emplace(buffer.data(), end);
    }
    return result;
}

/*  The text a reference stands for: the five XML has names for, plus
    the numeric ones, which a tagfile uses for anything outside plain
    ASCII.

    @param body What is between the `&` and the `;`.
    @param line The line the reference is on, named by the error.
*/
Expected<std::string>
expandedReference(std::string_view body, std::size_t line)
{
    Expected<std::string> result;
    if (std::optional<char> const named = namedEntity(body))
    {
        result = std::string(1, *named);
    }
    else if (!body.starts_with("#"))
    {
        result = Unexpected(formatError(
            "the entity reference \"&{};\" on line {} of a tagfile, "
            "where only the predefined ones and character references "
            "may appear", body, line));
    }
    else if (std::optional<std::string> character =
                 referencedCharacter(body.substr(1)))
    {
        result = *std::move(character);
    }
    else
    {
        result = Unexpected(formatError(
            "the character reference \"&{};\" on line {} of a tagfile, "
            "which is malformed or names no Unicode character", body, line));
    }
    return result;
}

/*  A scanner over the constructs a tagfile is written with.

    It reports the line it is on so that a file which is not one can say
    where it stopped being one.
*/
class Scanner
{
    std::string_view in_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;

public:
    explicit
    Scanner(std::string_view in)
        : in_(in)
    {
    }

    std::size_t
    line() const noexcept
    {
        return line_;
    }

    Expected<Token>
    next();

private:
    bool
    done() const noexcept
    {
        return pos_ >= in_.size();
    }

    char
    peek(std::size_t ahead = 0) const noexcept
    {
        return pos_ + ahead < in_.size() ? in_[pos_ + ahead] : '\0';
    }

    void
    advance()
    {
        if (peek() == '\n')
        {
            ++line_;
        }
        ++pos_;
    }

    bool
    match(std::string_view s) const noexcept
    {
        return in_.substr(pos_).starts_with(s);
    }

    void
    skip(std::size_t n)
    {
        while (n-- != 0 && !done())
        {
            advance();
        }
    }

    void
    skipSpace()
    {
        while (!done() && isSpace(peek()))
        {
            advance();
        }
    }

    std::string
    takeName();

    Expected<void>
    skipUntil(std::string_view terminator);

    Expected<void>
    skipIgnorableMarkup();

    Expected<void>
    appendReference(std::string& out);

    Expected<std::string>
    takeCharacterData(char stop);

    Expected<Token>
    takeText();

    Expected<std::string>
    takeAttributeValue();

    Expected<Attribute>
    takeAttribute();

    Expected<Token>
    takeOpenTag();

    Expected<Token>
    takeCloseTag();

    Expected<Token>
    takeTag();
};

std::string
Scanner::
takeName()
{
    std::string result;
    while (!done() && isNameChar(peek()))
    {
        result += peek();
        advance();
    }
    return result;
}

Expected<void>
Scanner::
skipUntil(std::string_view terminator)
{
    Expected<void> result;
    std::size_t const start = line_;
    while (!done() && !match(terminator))
    {
        advance();
    }
    if (done())
    {
        result = Unexpected(formatError(
            "no \"{}\" ends the markup on line {} of a tagfile",
            terminator, start));
    }
    else
    {
        skip(terminator.size());
    }
    return result;
}

/*  The markup a tagfile carries nothing in: the declaration it opens
    with, and comments, which may appear between any two tags. A
    construct that would need more of XML than this reads is an error,
    since nothing generating a tagfile emits one.
*/
Expected<void>
Scanner::
skipIgnorableMarkup()
{
    Expected<void> result;
    bool ignorable = true;
    while (ignorable && result.has_value())
    {
        if (match("<?"))
        {
            result = skipUntil("?>");
        }
        else if (match("<!--"))
        {
            skip(4);
            result = skipUntil("-->");
        }
        else if (match("<![CDATA["))
        {
            result = Unexpected(formatError(
                "a character data section on line {} of a tagfile", line_));
        }
        else if (match("<!"))
        {
            result = Unexpected(formatError(
                "a document type declaration on line {} of a tagfile",
                line_));
        }
        else
        {
            ignorable = false;
        }
    }
    return result;
}

// The one reference at the current position, expanded.
Expected<void>
Scanner::
appendReference(std::string& out)
{
    Expected<void> result;
    std::size_t const semicolon = in_.find(';', pos_);
    if (semicolon == std::string_view::npos)
    {
        result = Unexpected(formatError(
            "an unterminated entity reference on line {} of a tagfile",
            line_));
    }
    else
    {
        Expected<std::string> const text = expandedReference(
            in_.substr(pos_ + 1, semicolon - pos_ - 1), line_);
        if (text.has_value())
        {
            out += *text;
            skip(semicolon - pos_ + 1);
        }
        else
        {
            result = Unexpected(text.error());
        }
    }
    return result;
}

/*  A run of character data with the references in it expanded, ending
    at `stop` or at the end of the input, whichever comes first. The
    delimiter itself is left unread.
*/
Expected<std::string>
Scanner::
takeCharacterData(char stop)
{
    std::string text;
    while (!done() && peek() != stop)
    {
        if (peek() == '&')
        {
            MRDOCS_TRY(appendReference(text));
        }
        else
        {
            text += peek();
            advance();
        }
    }
    return text;
}

// The text between two tags.
Expected<Token>
Scanner::
takeText()
{
    Token token;
    token.kind = TokenKind::Text;
    MRDOCS_TRY(token.text, takeCharacterData('<'));
    return token;
}

/*  The value of an attribute, which is quoted either way and holds the
    same references text does.
*/
Expected<std::string>
Scanner::
takeAttributeValue()
{
    Expected<std::string> result;
    std::size_t const start = line_;
    char const quote = peek();
    if (quote != '"' && quote != '\'')
    {
        result = Unexpected(formatError(
            "an unquoted attribute value on line {} of a tagfile", line_));
    }
    else
    {
        advance();
        MRDOCS_TRY(std::string value, takeCharacterData(quote));
        if (done())
        {
            result = Unexpected(formatError(
                "an unterminated attribute value on line {} of a tagfile",
                start));
        }
        else
        {
            advance();
            result = std::move(value);
        }
    }
    return result;
}

// One `name="value"` pair.
Expected<Attribute>
Scanner::
takeAttribute()
{
    Expected<Attribute> result;
    Attribute attribute;
    attribute.name = takeName();
    skipSpace();
    if (attribute.name.empty())
    {
        result = Unexpected(formatError(
            "a malformed attribute on line {} of a tagfile", line_));
    }
    else if (peek() != '=')
    {
        result = Unexpected(formatError(
            "an attribute without a value on line {} of a tagfile", line_));
    }
    else
    {
        advance();
        skipSpace();
        MRDOCS_TRY(attribute.value, takeAttributeValue());
        result = std::move(attribute);
    }
    return result;
}

/*  An opening tag, which turns out to be an empty element if it ends in
    `/>`. Of its attributes only `kind` is kept: it is the one a tagfile
    reads anything from.
*/
Expected<Token>
Scanner::
takeOpenTag()
{
    Expected<Token> result;
    Token token;
    std::size_t const start = line_;
    token.kind = TokenKind::Open;
    token.name = takeName();
    bool complete = false;
    while (!complete && result.has_value())
    {
        skipSpace();
        if (done())
        {
            result = Unexpected(formatError(
                "an unterminated tag on line {} of a tagfile", start));
        }
        else if (peek() == '>')
        {
            advance();
            complete = true;
        }
        else if (peek() == '/' && peek(1) == '>')
        {
            skip(2);
            token.kind = TokenKind::Empty;
            complete = true;
        }
        else
        {
            MRDOCS_TRY(Attribute attribute, takeAttribute());
            if (attribute.name == "kind")
            {
                token.kindAttribute = std::move(attribute.value);
            }
        }
    }
    if (complete)
    {
        result = std::move(token);
    }
    return result;
}

// A closing tag, positioned just after its `</`.
Expected<Token>
Scanner::
takeCloseTag()
{
    Expected<Token> result;
    Token token;
    token.kind = TokenKind::Close;
    token.name = takeName();
    skipSpace();
    if (peek() != '>')
    {
        result = Unexpected(formatError(
            "a malformed closing tag on line {} of a tagfile", line_));
    }
    else
    {
        advance();
        result = std::move(token);
    }
    return result;
}

// A tag, positioned at its `<`.
Expected<Token>
Scanner::
takeTag()
{
    Expected<Token> result;
    advance(); // `<`
    if (peek() == '/')
    {
        advance();
        result = takeCloseTag();
    }
    else if (!isNameStart(peek()))
    {
        result = Unexpected(formatError(
            "a malformed tag on line {} of a tagfile", line_));
    }
    else
    {
        result = takeOpenTag();
    }
    return result;
}

Expected<Token>
Scanner::
next()
{
    Expected<Token> result;
    MRDOCS_TRY(skipIgnorableMarkup());
    if (done())
    {
        result = Token{};
    }
    else if (peek() == '<')
    {
        result = takeTag();
    }
    else
    {
        result = takeText();
    }
    return result;
}

// Whether a compound of this kind documents a symbol a reference can name.
bool
isRecordedKind(std::string_view kind)
{
    return is_one_of(
        kind, {"namespace", "class", "struct", "union", "concept"});
}

// The error for an element the tagfile ends inside of.
Error
leftOpen(std::string_view element, std::size_t line)
{
    return formatError(
        "\"{}\" is still open on line {}, where the tagfile ends",
        element, line);
}

// The text of an element known to hold nothing else, left at its close.
Expected<std::string>
readElementText(Scanner& scanner, std::string_view element)
{
    Expected<std::string> result;
    std::string text;
    bool open = true;
    while (open && result.has_value())
    {
        MRDOCS_TRY(Token token, scanner.next());
        if (token.kind == TokenKind::Text)
        {
            text += token.text;
        }
        else if (token.kind == TokenKind::Close && token.name == element)
        {
            open = false;
        }
        else if (token.kind == TokenKind::End)
        {
            result = Unexpected(leftOpen(element, scanner.line()));
        }
        else
        {
            result = Unexpected(formatError(
                "\"{}\" holds a \"{}\" on line {} of a tagfile",
                element, token.name, scanner.line()));
        }
    }
    if (result.has_value())
    {
        result = std::move(text);
    }
    return result;
}

// Everything up to the close of the element just opened, discarded.
Expected<void>
skipElement(Scanner& scanner, std::string_view element)
{
    Expected<void> result;
    std::size_t depth = 1;
    while (depth != 0 && result.has_value())
    {
        MRDOCS_TRY(Token token, scanner.next());
        if (token.kind == TokenKind::Open)
        {
            ++depth;
        }
        else if (token.kind == TokenKind::Close)
        {
            --depth;
        }
        else if (token.kind == TokenKind::End)
        {
            result = Unexpected(leftOpen(element, scanner.line()));
        }
    }
    return result;
}

/*  Each element inside the one just opened, in turn.

    An element the reader has no use for is skipped whole, so a tagfile
    may carry any number of them, holding anything, at no cost. What it
    must carry is the closing tag: a file that ends before it has been
    cut short, and is rejected rather than read as far as it goes.

    @param scanner The scanner, positioned just after the opening tag.
    @param element The name in that opening tag, whose closing tag ends
        the walk.
    @param onChild Called with the opening token of each element inside,
        and returns whether it read that element up to its close. One it
        leaves unread is skipped.
*/
template <class OnChild>
Expected<void>
readChildren(
    Scanner& scanner,
    std::string_view element,
    OnChild onChild)
{
    Expected<void> result;
    bool open = true;
    while (open && result.has_value())
    {
        MRDOCS_TRY(Token token, scanner.next());
        if (token.kind == TokenKind::Close && token.name == element)
        {
            open = false;
        }
        else if (token.kind == TokenKind::End)
        {
            result = Unexpected(leftOpen(element, scanner.line()));
        }
        else if (token.kind == TokenKind::Open)
        {
            MRDOCS_TRY(bool const handled, onChild(token));
            if (!handled)
            {
                result = skipElement(scanner, token.name);
            }
        }
    }
    return result;
}

struct Member
{
    std::string name;
    std::string anchorFile;
    std::string anchor;
};

// The field of a `Member` that an element of this name holds, or null.
std::string Member::*
memberField(std::string_view element)
{
    using Field = std::pair<std::string_view, std::string Member::*>;
    static constexpr Field fields[] = {
        {"name", &Member::name},
        {"anchorfile", &Member::anchorFile},
        {"anchor", &Member::anchor}
    };
    Field const* const entry =
        std::ranges::find(fields, element, &Field::first);
    return entry != std::ranges::end(fields) ? entry->second : nullptr;
}

// What a `<member>` says about the member it documents.
Expected<Member>
readMember(Scanner& scanner)
{
    Member member;
    MRDOCS_TRY(readChildren(scanner, "member",
        [&](Token const& token) -> Expected<bool>
        {
            std::string Member::* const field = memberField(token.name);
            if (field)
            {
                MRDOCS_TRY(member.*field,
                    readElementText(scanner, token.name));
            }
            return field != nullptr;
        }));
    return member;
}

struct Compound
{
    std::string name;
    std::string fileName;
    std::vector<Member> members;
};

// What a `<compound>` says about the scope it documents.
Expected<Compound>
readCompound(Scanner& scanner)
{
    Compound compound;
    MRDOCS_TRY(readChildren(scanner, "compound",
        [&](Token const& token) -> Expected<bool>
        {
            bool handled = true;
            if (token.name == "name")
            {
                MRDOCS_TRY(compound.name,
                    readElementText(scanner, token.name));
            }
            else if (token.name == "filename")
            {
                MRDOCS_TRY(compound.fileName,
                    readElementText(scanner, token.name));
            }
            else if (token.name == "member")
            {
                MRDOCS_TRY(Member member, readMember(scanner));
                compound.members.push_back(std::move(member));
            }
            else
            {
                handled = false;
            }
            return handled;
        }));
    return compound;
}

/*  The name a member is looked up by: qualified by its compound's name,
    except in the global namespace, which MrDocs writes with an empty
    name.
*/
std::string
qualifiedMemberName(std::string_view scope, std::string_view member)
{
    std::string result;
    if (!scope.empty())
    {
        result = scope;
        result += "::";
    }
    result += member;
    return result;
}

/*  The URL of a page, or of an anchor on it, in the documentation set
    published under `baseUrl`, with exactly one separator between the
    parts.
*/
std::string
makeUrl(
    std::string_view baseUrl,
    std::string_view page,
    std::string_view anchor)
{
    std::string url(baseUrl);

    // A configured base URL may or may not end with a slash, while a
    // page name read from a tagfile never begins with one.
    if (!url.empty() &&
        !url.ends_with('/'))
    {
        url += '/';
    }
    url += page;
    if (!anchor.empty())
    {
        url += '#';
        url += anchor;
    }
    return url;
}

/*  Record where one symbol is documented, unless the index knows the
    name already, whose first entry is the one kept, or the tagfile
    names no page for it: such an entry would link to the top of the
    documentation set rather than to the symbol.
*/
void
recordEntry(
    detail::TagfileIndex& index,
    std::string_view name,
    std::string_view baseUrl,
    std::string_view page,
    std::string_view anchor)
{
    if (!page.empty() && !index.contains(name))
    {
        index.insert(name, makeUrl(baseUrl, page, anchor));
    }
}

// What a compound and its members document, as index entries.
void
recordCompound(
    detail::TagfileIndex& index,
    std::string_view baseUrl,
    Compound const& compound)
{
    recordEntry(index, compound.name, baseUrl, compound.fileName, "");
    for (Member const& member: compound.members)
    {
        std::string const& page = member.anchorFile.empty()
            ? compound.fileName
            : member.anchorFile;
        recordEntry(
            index,
            qualifiedMemberName(compound.name, member.name),
            baseUrl,
            page,
            member.anchor);
    }
}

/*  Whether the first token that is not whitespace opens a root element
    with anything inside it.

    @param token The token.
    @param line The line the scanner is on after reading it.
*/
Expected<bool>
opensRootElement(Token const& token, std::size_t line)
{
    Expected<bool> result = false;
    if (token.kind == TokenKind::Text)
    {
        result = Unexpected(formatError(
            "text where a tagfile begins, on line {}", line));
    }
    else if (token.kind != TokenKind::End && token.name != "tagfile")
    {
        result = Unexpected(formatError(
            "\"{}\" where a tagfile begins, on line {}", token.name, line));
    }
    else
    {
        // An empty root element holds no compounds.
        result = token.kind == TokenKind::Open;
    }
    return result;
}

// The contents without the byte order mark an editor may put first.
std::string_view
withoutByteOrderMark(std::string_view contents)
{
    std::string_view const bom = "\xEF\xBB\xBF";
    return contents.starts_with(bom) ? contents.substr(bom.size()) : contents;
}

/*  The opening tag of the root element, which a tagfile names `tagfile`.

    @param scanner The scanner, positioned at the start of the file.
    @return Whether there is anything inside it: a file with no elements
        at all documents nothing, and so does one whose root is written
        `<tagfile/>`.
*/
Expected<bool>
enterTagfileElement(Scanner& scanner)
{
    Expected<bool> result = false;
    bool looking = true;
    while (looking && result.has_value())
    {
        MRDOCS_TRY(Token token, scanner.next());
        looking = token.kind == TokenKind::Text && isWhitespace(token.text);
        if (!looking)
        {
            result = opensRootElement(token, scanner.line());
        }
    }
    return result;
}

} // (anon)

Expected<void>
readTagfile(
    detail::TagfileIndex& index,
    std::string_view contents,
    std::string_view baseUrl)
{
    Expected<void> result;
    Scanner scanner(withoutByteOrderMark(contents));
    MRDOCS_TRY(bool const inTagfile, enterTagfileElement(scanner));
    if (inTagfile)
    {
        result = readChildren(scanner, "tagfile",
            [&](Token const& token) -> Expected<bool>
            {
                // A compound of a kind no reference can name is skipped
                // whole.
                bool const handled =
                    token.name == "compound" &&
                    isRecordedKind(token.kindAttribute);
                if (handled)
                {
                    MRDOCS_TRY(Compound compound, readCompound(scanner));
                    recordCompound(index, baseUrl, compound);
                }
                return handled;
            });
    }
    return result;
}

Expected<void>
loadTagfile(
    detail::TagfileIndex& index,
    std::string_view path,
    std::string_view baseUrl)
{
    Expected<void> result;
    MRDOCS_TRY(std::string const contents, files::getFileText(path));
    Expected<void> const read = readTagfile(index, contents, baseUrl);
    if (!read.has_value())
    {
        result = Unexpected(formatError(
            "{}: {}", path, read.error().reason()));
    }
    return result;
}

} // mrdocs
