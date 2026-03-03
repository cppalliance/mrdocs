/** Type with only operator(): auto-detected without the command. */
struct auto_detected_fn
{
    /** Call operator.
        @return A value.
    */
    int
    operator()() const;
};

constexpr auto_detected_fn auto_detected = {};

/** Type with extra public members: not auto-detected.
    The @functionobject command forces treatment.
*/
struct forced_fn
{
    /** Call operator.
        @return A value.
    */
    int
    operator()() const;

    /** Extra public member that prevents auto-detection. */
    char const*
    name() const;
};

/** @functionobject
    Forced function object despite extra public members.
*/
constexpr forced_fn forced = {};

/** Documentation on the struct, not the variable.
    The variable has no doc comment of its own.
*/
struct doc_on_struct_fn
{
    /** Call operator.
        @return A value.
    */
    int
    operator()() const;
};

constexpr doc_on_struct_fn doc_on_struct = {};

struct undocumented_fn
{
    /** Call operator.
        @return A value.
    */
    int
    operator()() const;
};

/** Documentation on the variable, not the struct. */
constexpr undocumented_fn doc_on_variable = {};
