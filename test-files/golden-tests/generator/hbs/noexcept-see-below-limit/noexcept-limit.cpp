/** Swaps two values, conditionally noexcept.

    @param a first value
    @param b second value
 */
template <class T>
void swap(T& a, T& b) noexcept(sizeof(T) > sizeof(int));
