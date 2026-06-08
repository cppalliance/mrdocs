/** Computes a hash code for a byte sequence.

    Typical use:

    @code
    auto h = compute_hash("hello", 5);
    @endcode
 */
unsigned compute_hash(const char* data, unsigned n);
