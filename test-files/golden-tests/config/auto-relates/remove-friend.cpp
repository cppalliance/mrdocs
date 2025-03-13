/** A record with non-member functions
 */
class A {
public:
    /** Friend function not listed as non-member

        Friends are already listed in the class definition.

        @param lhs The left-hand side of the comparison
        @param rhs The right-hand side of the comparison
        @return `true` if the objects are equal
     */
    friend
    bool
    operator==(A const& lhs, A const& rhs);
};

/** Non-member function of A

    @param a The object to stringify
    @return The string representation of the object
 */
char const*
to_string(A const& a);