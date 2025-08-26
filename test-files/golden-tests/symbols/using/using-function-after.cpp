namespace A
{
    /// A shadow declaration
    void f(int);
}

/// f overloads are also available in the global namespace
using A::f;

namespace A
{
    /** A non-shadowing declaration

        This declaration is not part
        of the shadow declarations
        of the using directive because
        it comes after it.

        Only the declarations at
        the point of the using directive
        are considered for shadowing in the
        documentation.
     */
    void f(char);
}
