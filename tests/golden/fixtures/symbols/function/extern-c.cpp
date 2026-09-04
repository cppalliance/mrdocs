// Verify that symbols wrapped in `extern "C"` are extracted. C library
// headers (OpenSSL, zlib, …) put their entire public API inside a linkage
// spec; skipping that DeclContext used to leave the corpus empty.

/** Add two integers.
    @param a First operand.
    @param b Second operand.
    @return The sum.
*/
extern "C" int c_add(int a, int b);

extern "C" {

/** A C-linkage state record. */
struct c_state {
    /** Stored value. */
    int value;
};

/** Reset a state.
    @param s The state to reset.
*/
void c_reset(struct c_state* s);

}
