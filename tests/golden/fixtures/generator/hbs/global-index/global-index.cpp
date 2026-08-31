// A C-style library (or a C++ one that uses no namespaces): the global scope
// has no nested namespaces, so its page heading reads "Global index" rather
// than "Global namespace" (there is no namespace concept to speak of).

/// A point.
struct point { int x; int y; };

/// Reset a point to the origin.
void reset(struct point* p);
