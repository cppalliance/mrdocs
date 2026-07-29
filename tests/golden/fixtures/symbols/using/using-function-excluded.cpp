namespace A
{
    // This symbol is excluded.
    void f(int);
}

/** Using excluded function f

    No shadow should be listed because A::f is excluded.

    Only included symbols are listed in the shadow list.
 */
using A::f;

