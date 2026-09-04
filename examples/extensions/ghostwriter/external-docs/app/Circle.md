A `Circle` is defined entirely by its radius; its center is supplied by the
surrounding coordinate system rather than stored here. This keeps the type
small enough to pass by value.

The radius is assumed non-negative. Passing a negative radius is a precondition
violation, not a runtime error, so the geometry helpers do not check for it.
