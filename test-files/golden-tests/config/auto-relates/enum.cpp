/** An enum with non-member functions
 */
enum class E {};

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
E makeE();

/** Function that returns template on A

    @return An instance of A or an error
 */
Result<E> tryMakeE();

/** Function that returns template on A

    @return A vector of As
 */
SmallVector<E, 3> makeEs();

