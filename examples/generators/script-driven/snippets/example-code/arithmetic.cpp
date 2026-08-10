#include "arithmetic.hpp"
#include <cassert>

void examples()
{
    //[multiply
    int p = app::multiply(4, 5);
    assert(p == 20);
    //]

    //[multiply
    int q = app::multiply(-2, 3);
    assert(q == -6);
    //]

    //[subtract
    int d = app::subtract(9, 4);
    assert(d == 5);
    //]
}
