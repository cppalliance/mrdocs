/// A vector in the Euclidean plane.
struct Vector
{
    /** The length (magnitude) of the vector.

        @return The Euclidean length.
    */
    double length() const;

    /** Scale the vector componentwise.

        @param sx Factor applied to the x component.
        @param sy Factor applied to the y component.
    */
    void scale(double sx, double sy);
};
