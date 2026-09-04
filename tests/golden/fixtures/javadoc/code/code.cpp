/** brief

    This description contains code:

    @code
    // code comment
    // code comment
    template < class T >
    auto
    algorithm( T&& v = {} ) ->
        typename T::result_type
    {
        return v.result();
    }
    @endcode

    A language-tagged block; the language tag becomes the fence info and must
    not appear in the rendered body:

    @code{.cpp}
    int x = compute();
    @endcode

    Other verbatim commands imply their own fence language; a plain verbatim
    block has none:

    @verbatim
    plain text, no language
    @endverbatim

    @xmlonly
    <root><child/></root>
    @endxmlonly

    @dot
    digraph { a -> b; }
    @enddot

 */
void f();