/** Encodes a byte sequence as base-64.

    @param  data The bytes to encode.
    @param  n    Number of bytes in `data`.
    @returns A null-terminated base-64 string. Owned by the caller.
 */
char* b64_encode(const char* data, unsigned n);

/** @copydoc b64_encode */
char* base64(const char* data, unsigned n);
