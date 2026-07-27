/** A cache with a bounded number of entries[^lru].

    A lookup that misses falls through to the backing store[^lru], and a
    second policy governs write-back[^wb].

    [^lru]: Least-recently-used entries are evicted first.

    [^wb]: Dirty entries are flushed lazily.
 */
struct cache { };
