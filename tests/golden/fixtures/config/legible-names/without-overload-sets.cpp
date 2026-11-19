// Without overload sets, the function called with no arguments keeps the
// plain name, and the others add the number of their parameters.
struct Widget
{
    void resize();
    void resize(int n);
    void resize(int n, int m);
};
