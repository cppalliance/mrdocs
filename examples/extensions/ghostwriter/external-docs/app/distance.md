`distance` returns the Euclidean distance between two points: the length of the
straight segment joining them, or equivalently the square root of the summed
squared differences of their `x` and `y` components.

The value is symmetric in its arguments and is zero exactly when the two points
coincide, so it works well as the basis for an equality-with-tolerance check.
