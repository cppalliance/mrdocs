/// Struct used to vary the parameter type.
template <int Idx>
struct paramType {};

/** Reference function.

    Documentation for the reference function.
 */
void
f();

/** @copydoc f()

    This function uses a reference with no
    parameters.

    @param a The first parameter of f
 */
void
f(paramType<0> a);

/** @copydoc f(void)

    This reference uses the `void` keyword.

    @param a The first parameter of f
 */
void
f(paramType<1> a);

/** Variadic function

    Documentation for the variadic function.

    @param a The first parameter of g
 */
void
g(int a, ...);

/** Non-variadic function

    Documentation for the non-variadic function.

    @param a The first parameter of g
 */
void
g(int a);

/** @copydoc g(int a, ...)

    This reference uses the `...` keyword.
 */
void
f(paramType<2> a);

/** @copydoc g(int a)

    This reference uses the `int` keyword.
 */
void
f(paramType<3> a);

/// Struct to test explicit object member functions.
struct A {
    /** Reference member function.

        Documentation for a function with an
        explicit object parameter.

        @param self The object to operate on
        @param a The first parameter of g
     */
    void
    g(this A& self, int a);

    /** @copydoc g(this A& self, int a)

        This reference uses the `this` keyword.
     */
    void
    f(paramType<4> a);
};

/** Auto function

    Documentation for a function with an `auto` parameter.

    @param a The first parameter of g
 */
void
g(auto a);

/** @copydoc g(auto a)

    This reference uses the `auto` keyword.
 */
void
f(paramType<5> a);

/** Decltype function

    Documentation for a function with a `decltype` parameter.

    @param a The first parameter of g
    @param b The second parameter of g
 */
void
g(int a, decltype(a) b);

/** @copydoc g(int a, decltype(a) b)

    This reference uses the `decltype` keyword.
 */
void
f(paramType<6> a);

/** struct param function

    Documentation for a function with a struct parameter.

    @param a The first parameter of g
 */
void
g(struct A a);

/** @copydoc g(struct A a)

    This reference uses the `struct` keyword.
 */
void
f(paramType<7> a);

enum testEnum {};

/** Enum param function

    Documentation for a function with an enum parameter.

    @param a The first parameter of g
 */
void
g(enum testEnum a);

/** @copydoc g(enum testEnum a)

    This reference uses the `enum` keyword.
 */
void
f(paramType<8> a);

/// Namespace to test qualified identifier parameters.
namespace N {
    /// Namespace to test qualified identifier parameters.
    namespace M {
        /// Struct to test qualified identifier parameters.
        struct Q {};
    }
}

/** Qualified identifier param function

    Documentation for a function with a qualified identifier parameter.

    @param a The first parameter of g
 */
void
g(N::M::Q a);

/** @copydoc g(N::M::Q a)

    This reference uses the qualified identifier `N::M::Q`.
 */
void
f(paramType<9> a);
