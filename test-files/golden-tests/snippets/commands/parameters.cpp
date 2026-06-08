/** Copies bytes from one buffer to another.

    @param[out] dst The destination buffer.
                    Must be at least `n` bytes long.
    @param[in]  src The source buffer.
    @param[in]  n   The number of bytes to copy.
 */
void copy_bytes(char* dst, const char* src, unsigned n);
