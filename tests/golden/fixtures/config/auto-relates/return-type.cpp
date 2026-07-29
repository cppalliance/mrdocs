/** A class with non-member functions
 */
class A {};

/** Helper result class
 */
template <class T>
class Result {};

/** Helper result class
 */
template <class T, unsigned long N>
class SmallVector {};

/** Function that returns A

    @return An instance of A
 */
A makeA();

/** Function that returns template on A

    @return An instance of A or an error
 */
Result<A> tryMakeA();

/** Function that returns template on A

    @return A vector of As
 */
SmallVector<A, 3> makeAs();

