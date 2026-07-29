namespace detail {
/** An opaque RAII token.

    @implementationdefined
 */
struct scope_token { ~scope_token(); };
}

/** Attach a key/value pair to the current logging scope.

    @return An opaque token whose type is implementation-defined;
            store it in `auto`.
 */
detail::scope_token scoped_context(char const* key, char const* value);
