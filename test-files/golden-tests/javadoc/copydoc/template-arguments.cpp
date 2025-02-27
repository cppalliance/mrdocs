namespace A {
    /** Main class template for B.

         @tparam T The type parameter.
         @tparam int The integer parameter with a default value of 0.
     */
    template <class T, int = 0>
    struct B;

    /// @copydoc B
    template <class T, int I>
    using B_t = B<T, I>;

    /** Specialization of B for int.
     */
    template <>
    struct B<int> {};

    /// @copydoc B<int>
    using BInt = B<int>;

    /** Specialization of B for int with value 2.
     */
    template <>
    struct B<int, 2> {};

    /// @copydoc B<int, 2>
    using BInt2 = B<int, 2>;

    /** Main class template for C.

         @tparam T The type parameter.
         @tparam bool The boolean parameter with a default value of false.
     */
    template <class T, bool = false>
    struct C;

    /// @copydoc C
    template <class T, bool B>
    using C_t = C<T, B>;

    /** Specialization of C for int with true.
     */
    template <>
    struct C<int, true> {};

    /// @copydoc C<int, true>
    using CIntTrue = C<int, true>;

    /** Helper struct D.
     */
    struct D;

    /** Specialization of C for D with true.
     */
    template <>
    struct C<D, true> {};

    /// @copydoc C<D, true>
    using CDTrue = C<D, true>;
}