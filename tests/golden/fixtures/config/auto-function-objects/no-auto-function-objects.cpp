namespace example {

/** Would be auto-detected, but auto-function-objects is false.
    Should remain a regular variable.
*/
struct auto_fn
{
    /** Function call operator.
        \return Something.
    */
    int
    operator()() const;
};

constexpr auto_fn auto_var = {};

/** Explicitly marked with @functionobject.
    Should still be treated as a function object even when
    auto-detection is off.
*/
struct explicit_fn
{
    /** Function call operator.
        \return Something.
    */
    int
    operator()() const;
};

/** @functionobject
    Explicit function object.
*/
constexpr explicit_fn explicit_var = {};

} // namespace example
