/// Helper class for distinct parameter types
template <int N>
class param_t;

namespace N {
    struct A {
        struct B {};

        /** Fail

            Function with same number of parameters
            but different types. This should fail.

            @param a Fundamental type parameter
         */
        void g(int a);

        /** Reference function

            Documentation for the reference function

            @param a Qualified param
         */
        void g(B a);

        /// @copydoc g(B)
        void f(param_t<0> a);

        /// @copydoc g(A::B)
        void f(param_t<1> a);

        /// @copydoc g(N::A::B)
        void f(param_t<2> a);

        /// @copydoc g(::N::A::B)
        void f(param_t<3> a);

        /** Fail

            Function with same number of parameters
            but different types. This should fail.

            @param a Fundamental type parameter
         */
        void h(int a);

        /** Reference function

            Documentation for the reference function

            @param a Qualified param
         */
        void h(::N::A::B a);

        /// @copydoc h(B)
        void f(param_t<4> a);

        /// @copydoc h(A::B)
        void f(param_t<5> a);

        /// @copydoc h(N::A::B)
        void f(param_t<6> a);

        /// @copydoc h(::N::A::B)
        void f(param_t<7> a);
    };
}