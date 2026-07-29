//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_OUTPUTREF_HPP
#define MRDOCS_API_HANDLEBARS_OUTPUTREF_HPP

// The OutputRef output-stream adapter and the HTMLEscape helpers.

#include <mrdocs/Handlebars/Platform.hpp>
#include <mrdocs/Handlebars/detail/OutputRef.hpp>
#include <concepts>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mrdocs {
namespace handlebars {

/** Reference to output stream used by handlebars

    This class is used to internally pass an output stream to the
    handlebars environment.

    It allows many types to be used as output streams, including
    std::string, std::ostream, llvm::raw_string_ostream, and others.

*/
class MRDOCS_HANDLEBARS_DECL OutputRef
{
    friend class Handlebars;

    using fptr = void (*)(void * out, std::string_view sv);
    void * out_;
    fptr fptr_;
    std::size_t indent_ = 0;

    template<class St>
    static
    void
    append_to_output(void * out, std::string_view sv) {
        St& st = *static_cast<St*>( out );
        st.append( sv.data(), sv.data() + sv.size() );
    }

    // write to output
    template<class Os>
    static
    void
    write_to_output(void * out, std::string_view sv) {
        Os& os = *static_cast<Os*>( out );
        os.write( sv.data(), sv.size() );
    }

    // stream to output
    template<class Os>
    static
    void
    stream_to_output(void * out, std::string_view sv) {
        Os& os = *static_cast<Os*>( out );
        os << sv;
    }

    // stream to output
    static
    void
    noop_output(void *, std::string_view) {}

    // Noop constructor
    // Used as implementation detail by Handlebars environment
    OutputRef()
        : out_( nullptr )
        , fptr_( &noop_output )
    {}

    OutputRef&
    write_impl( std::string_view sv );

public:
    /** Constructor for std::string output

        @param st The string to append to
    */
    template<detail::SVAppendable St>
    requires std::same_as<typename St::value_type, char>
    OutputRef( St& st )
      : out_( &st )
      , fptr_( &append_to_output<St> )
    {
    }

    /** Constructor for llvm::raw_string_ostream output

        @param os The output stream to write to
    */
    template <detail::StdLHROStreamable Os>
    requires std::is_convertible_v<Os*, std::ostream*>
    OutputRef( Os& os )
        : out_( &os )
        , fptr_( &write_to_output<Os> )
    {
    }

    /** Constructor for std::ostream& output

        @param os The output stream to write to
    */
    template <detail::LHROStreamable Os>
    requires
        std::is_convertible_v<Os*, std::ostream*> &&
        (!detail::StdLHROStreamable<Os>)
    OutputRef( Os& os )
        : out_( &os )
        , fptr_( &write_to_output<Os> )
    {
    }

    /** Write to output

        @param os The output stream reference
        @param sv The string to write
        @return A reference to this object
    */
    friend
    OutputRef&
    operator<<( OutputRef& os, std::string_view sv )
    {
        return os.write_impl( sv );
    }

    /** Write to output

        @param os The output stream reference
        @param c The character to write
        @return A reference to this object
    */
    friend
    OutputRef&
    operator<<( OutputRef& os, char c )
    {
        return os.write_impl( std::string_view( &c, 1 ) );
    }

    /** Write to output

        @param os The output stream reference
        @param c The string to write
        @return A reference to this object
    */
    friend
    OutputRef&
    operator<<( OutputRef& os, char const * c )
    {
        return os.write_impl( std::string_view( c ) );
    }

    /** Write to output

        @param os The output stream reference
        @param v The character to write
        @return A reference to this object
    */
    template <class T>
      requires std::formattable<T, char>
    friend OutputRef &operator<<(OutputRef &os, T v) {
      std::string s = std::format("{}", v);
      return os.write_impl(s);
    }

    /** Set the indentation level applied to writes.
        @param indent Number of spaces to indent.
    */
    void
    setIndent(std::size_t indent)
    {
        indent_ = indent;
    }

    /** Return the current indentation level.
    */
    std::size_t
    getIndent() const noexcept
    {
        return indent_;
    }
};

/** HTML escapes the specified string

    This function HTML escapes the specified string, making it safe for
    rendering as text within HTML content.

    Replaces `&`, `<`, `>`, `"`, `'`, ```, `=` with the HTML entity
    equivalent value for string values.

    The output of all expressions except for triple-braced expressions
    are passed through this method. Helpers should also use this method
    when returning HTML content via a SafeString instance, to prevent
    possible code injection.

    Helper values created by the SafeString function are left untouched
    by the template and are not passed through this function.

    This function has the same behavior as the corresponding utility function
    in the Handlebars.js library.

    @see https://github.com/handlebars-lang/handlebars.js/blob/master/lib/handlebars/utils.js

    @param out The output stream reference where the escaped string will be written.
    @param str The string to escape.
*/
MRDOCS_HANDLEBARS_DECL
void
HTMLEscape(
    OutputRef& out,
    std::string_view str);

/** Character-to-entity table used by `HTMLEscape`.
*/
inline constexpr std::pair<char, std::string_view>
htmlEscapeEntities[] = {
    {'&',  "&amp;"},
    {'<',  "&lt;"},
    {'>',  "&gt;"},
    {'"',  "&quot;"},
    {'\'', "&#x27;"},
    {'`',  "&#x60;"},
    {'=',  "&#x3D;"}
};

/** \brief HTML escapes the specified string.
 *
 * This function HTML escapes the specified string, making it safe for
 * rendering as text within HTML content.
 *
 * Replaces `&`, `<`, `>`, `"`, `'`, ```, `=` with the HTML entity
 * equivalent value for string values.
 *
 * The output of all expressions except for triple-braced expressions
 * are passed through this method. Helpers should also use this method
 * when returning HTML content via a SafeString instance, to prevent
 * possible code injection.
 *
 * Helper values created by the SafeString function are left untouched
 * by the template and are not passed through this function.
 *
 * \param str The string to escape.
 * \return The escaped string.
*/
MRDOCS_HANDLEBARS_DECL
std::string
HTMLEscape(std::string_view str);

} // namespace handlebars
} // namespace mrdocs

#endif
