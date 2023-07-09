/** The type of string_view used by the library

    String views are used to pass character
    buffers into or out of functions. Ownership
    of the underlying character buffer is not
    transferred; the caller is responsible for
    ensuring that the lifetime of character
    buffer extends until it is no longer
    referenced.

    @note This alias is no longer supported and
    should not be used in new code. Please use
    `core::string_view` instead.

    This alias is included for backwards
    compatibility with earlier versions of the
    library.

    However, it will be removed in future releases,
    and using it in new code is not recommended.

    Please use the updated version instead to
    ensure compatibility with future versions of
    the library.

*/
struct string_view{};
