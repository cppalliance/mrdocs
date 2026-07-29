#ifndef JZON_PARSER_HPP
#define JZON_PARSER_HPP

// tag::body[]
namespace jzon {

/** Validate that the document is well-formed JSON.

    Parses `source` without constructing a value tree, reporting only
    whether the input conforms to RFC 8259. Comments and trailing
    commas are rejected.

    @param source A null-terminated UTF-8 JSON document.
    @return `true` if the document is syntactically valid JSON, `false`
            if any syntactic error is encountered.
*/
bool validate(char const* source);

}
// end::body[]

#endif
