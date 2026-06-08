/** @brief A fast hash function over byte sequences.

    Suitable for hash tables but not for cryptographic use.
 */
unsigned fast_hash(const char* bytes, unsigned n);

/** @copybrief fast_hash

    This implementation uses a wider accumulator and is
    measurably faster on long inputs.
 */
unsigned fast_hash_v2(const char* bytes, unsigned n);
