#include <system.hpp>
#include "../outside/outside.hpp"

/** Return the sum of the two headers' functions. */
int
main_sum()
{
    return sys::g(1) + beforeOutside() + fromOutside();
}

// The definition makes the visitor look at the sections of the system header.
int
sys::g(int x)
{
    return x;
}
