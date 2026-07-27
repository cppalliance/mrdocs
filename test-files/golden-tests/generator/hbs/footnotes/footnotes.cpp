/** A cache with a bounded number of entries[^lru].

    A lookup that misses falls through to the backing store[^lru].

    [^lru]: Least-recently-used entries are evicted first.
 */
struct cache { };
