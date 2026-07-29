/** Sorts the elements of `buffer` in ascending order.

    @post For every index i in [0, n - 1),
          `buffer[i] <= buffer[i + 1]`.
 */
void sort_buffer(int* buffer, unsigned n);
