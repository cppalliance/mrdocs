/** @brief A function with a single return value.

    @return The return value of the function.
 */
int f();

template <class T, class U>
struct pair {
    T first;
    U second;
};

/** @brief A function with multiple return values.

    @return The first return value of the function.
    @return The second return value of the function.
 */
pair<int, int> g();

