#ifndef JZON_PARSER_HPP
#define JZON_PARSER_HPP

// tag::body[]
namespace jzon {

/// A parsed JSON document.
class value;

/** Validate that the document is well-formed JSON.

    Parses `source` without constructing a value tree, reporting only
    whether the input conforms to RFC 8259.

    @param source A null-terminated UTF-8 JSON document.
    @return `true` if the document is syntactically valid JSON.
*/
bool validate(char const* source);

/** Parse a JSON document.

    Validates `source` and returns the resulting value tree.

    @param source A null-terminated UTF-8 JSON document.
*/
value parse(char const* source);

/** Parse a JSON document without validating.

    Assumes the caller has already verified `source` with @ref validate.
    The behavior is undefined if `source` is not well-formed JSON.
*/
value unsafe_parse(char const* source);

/** Count the number of top-level elements in `source`.

    Assumes the caller has already verified `source` with @ref validate.
*/
unsigned long unsafe_element_count(char const* source);

}
// end::body[]

#endif
