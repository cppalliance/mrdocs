/** A class with non-member functions
 */
struct A {};

/** A non-member function of A
 */
void f1(A const&);

namespace N {
    /** A non-member function of A
     */
    void f2(A&);

    /** A nested class with non-member functions
     */
    struct B {};

    /** A non-member function of B
    */
    void f3(::N::B const&);

    namespace M {
        /** A non-member function of ::N::B
         */
        void f4(::N::B const&);
    }
}

/** A non-member function of ::N::B
 */
void f5(::N::B const&);

namespace O {
    /** A non-member function of ::N::B
     */
    void f6(::N::B const&);
}