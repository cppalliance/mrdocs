/** Take an array by reference with dependent bounds.
    \param arr An array.
*/
template<typename T, unsigned long N>
void f(T (&arr)[N]);

/** Take an array by reference to const with dependent bounds.
    \param arr An array.
*/
template<typename T, unsigned long N>
void g(T const (&arr)[N]);
