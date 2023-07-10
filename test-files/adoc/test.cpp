/// Nothing much
struct U {};

struct T
{
    /** Append a header

        This function appends a new header.
        Existing headers with the same name are
        not changed. Names are not case-sensitive.
        <br>
        No iterators are invalidated.

        @par Example
        @code
        request req;

        req.append( field::user_agent, "Boost" );
        @endcode

        @par Complexity
        Linear in `to_string( id ).size() + value.size()`.

        @par Exception Safety
        Strong guarantee.
        Calls to allocate may throw.

        @param id The field name constant,
        which may not be @ref U.

        @param value A value, which
        @li Must be syntactically valid for the header,
        @li Must be semantically valid for the message, and
        @li May not contain leading or trailing whitespace.

        @return nothing.
    */
    void append(int id, char const* value);
};
