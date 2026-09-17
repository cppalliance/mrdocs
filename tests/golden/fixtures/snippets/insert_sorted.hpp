#include <vector>

/** Insert a value into a sorted vector, keeping it sorted.

    @pre `v` is sorted in ascending order.
    @post `v` is sorted in ascending order and contains `value`.

    @param v The sorted vector to insert into.
    @param value The value to insert.
    @return An iterator to the inserted element.
    @throws std::bad_alloc If the vector has to grow and cannot.
*/
std::vector<int>::iterator
insert_sorted(std::vector<int>& v, int value);
