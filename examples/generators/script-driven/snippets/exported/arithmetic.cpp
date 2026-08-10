#include "arithmetic.hpp"
#include <cassert>

void add_snippet()
{
    int s = app::add(2, 3);
    assert(s == 5);
}

int main()
{
    add_snippet();
}
