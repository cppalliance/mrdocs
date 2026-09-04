#include "text.hpp"
#include <cassert>

void length_snippet()
{
    assert(app::length("abc") == 3);
}

void is_empty_snippet()
{
    {
        assert(app::is_empty(""));
    }
    {
        assert(!app::is_empty("abc"));
    }
}

int main()
{
    length_snippet();
    is_empty_snippet();
}
