/** A pixel record. */
struct pixel { int x, y; };

/** Computes the Euclidean distance between two pixels.

    @relates pixel
 */
double distance(pixel const& a, pixel const& b);
