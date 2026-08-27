// Merging two DocComments fills each field from the second declaration only
// when the first lacks it, instead of appending everything. Here both
// redeclarations of `add` carry the same brief and the second adds the params
// and return: the merged doc must hold a single brief (not a repeated one) plus
// the params and return.

/// Adds two numbers.
int add(int a, int b);

/// Adds two numbers.
/// \param a The first addend.
/// \param b The second addend.
/// \return The sum.
int add(int a, int b);
