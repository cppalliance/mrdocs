/** Test function

    @throw std::runtime_error Describe the exception

    @pre a must be greater than 0
    @post The return value is greater than 0

    @tparam A Describe A
    @param a Describe a
    @return The number corresponding to a

    @see Interesting information 1
 */
template <class A>
int
f(int a);

/** Test function

    @throw std::logic_error Describe the exception

    @pre a must be greater than 0
    @pre b must be greater than 0
    @post The return value is greater than 10

    @tparam A Describe A again
    @tparam B Describe B
    @param a Describe a again
    @param b Describe b
    @return The number corresponding to a and b
 */
template <class A, class B>
int
f(int a, int b);

/** Test function

    @param a Describe a again
    @return The number corresponding to a

    @see Interesting information 2
 */
int
f(int a, void*);

