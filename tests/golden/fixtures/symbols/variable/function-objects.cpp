namespace external {

/** A class template from an unrelated namespace.
    Auto-detection should skip it because templates from
    non-enclosing scopes are excluded (prevents false
    positives like std::hash<T>).
*/
template<class T>
struct hash_like
{
    T operator()(T x) const;
};

} // namespace external

namespace example {

/** Auto-detected: single operator(), no other public non-special members. */
struct single_overload_fn
{
    /** The single overload.
        \return A Boolean.
    */
    bool
    operator()() const;
};

constexpr single_overload_fn single_overload = {};

/** Auto-detected: multiple operator() overloads, no other
    public non-special members. Both overloads should appear in
    Synopsis.
*/
struct multi_overload_fn
{
    /** First overload.
        \return A number.
    */
    int
    operator()() const;

    /** Second overload.
        \param begin The beginning of the range.
        \param end The end of the range.
        \return A number.
    */
    int
    operator()(char const* begin, char const* end) const;
};

constexpr multi_overload_fn multi_overload = {};

/** An ordinary function for comparison. */
void
ordinary_function();

/** An ordinary type for comparison. */
struct ordinary_type {};

/** Not auto-detected: has other public non-special members
    besides operator(). Variable stays in the Variables tranche.
*/
struct not_detected_fn
{
    void operator()() const;
    void other() const;
};

constexpr not_detected_fn not_detected = {};

/** Auto-detected template: same-parent scope heuristic.

    Templated structs are auto-detected when the type
    lives in a scope enclosed by the variable's parent
    scope. Types from unrelated scopes (like std::hash)
    are excluded.
*/
template<class T>
struct template_fn
{
    /** Template overload.
        @param val The value to process.
        @return The result.
    */
    int
    operator()(T const& val) const;
};

template<class T>
constexpr template_fn<T> template_fo = {};

namespace detail {

/** Auto-detected template: struct in a ::detail
    sub-namespace, variable in the parent namespace.

    The enclosing-scope heuristic auto-detects this pattern.
*/
template<class Pred>
struct detail_fn
{
    /** Detail overload.
        @return The result.
    */
    int
    operator()() const;
};

} // namespace detail

template<class Pred>
constexpr detail::detail_fn<Pred> detail_fo = {};

/** Explicit @functionobject doc command: the type has a
    non-operator() public member (name()), so it would not
    be auto-detected. The command forces treatment.
*/
struct explicit_fn
{
    /** Explicit overload.
        @return The result.
    */
    int
    operator()() const;

    /** Extra public member that prevents auto-detection. */
    char const*
    name() const;
};

/** @functionobject
    Explicitly marked function object.
*/
constexpr explicit_fn explicit_fo = {};

/** A class that contains a function object as a static member. */
struct host
{
    struct invoke_fn
    {
        /** Invoke the operation.
            @return The result.
        */
        int
        operator()() const;
    };

    /** A static function object member. */
    static invoke_fn invoke;
};

/** Auto-detected: unnamed struct type with operator(). */
constexpr struct
{
    /** Unnamed overload.
        @return The result.
    */
    int
    operator()() const;
} unnamed_fo = {};

/** Same template implementation, different variable instantiations.

    This pattern is common in real-world libraries: one template
    implementation with multiple variable instantiations for
    different type parameters.
*/
template<class T>
struct compare_fn
{
    /** Compare two values.
        @param a The first value.
        @param b The second value.
        @return True if a is less than b.
    */
    bool
    operator()(T const& a, T const& b) const;
};

/** Compare integers. */
constexpr compare_fn<int> compare_int = {};

/** Compare doubles. */
constexpr compare_fn<double> compare_double = {};

struct undocumented_struct_fn
{
    /** Call.
        @return Something.
    */
    int
    operator()() const;
};

/** Documentation on the variable, not on the struct. */
constexpr undocumented_struct_fn doc_on_var = {};

/** Documented struct, undocumented variable.

    When the struct carries the documentation but the
    variable does not, the struct doc becomes hidden
    (implementation-defined) and the variable has no brief.
*/
struct documented_struct_fn
{
    /** Call.
        @return Something.
    */
    int
    operator()() const;
};

constexpr documented_struct_fn doc_on_struct = {};

namespace impl {

/** Implementation type in a nested namespace.

    The struct lives in impl::, but the variable is exposed
    in the parent namespace. Tests visibility mismatch between
    the implementation type and the public variable.
*/
struct hidden_fn
{
    /** Hidden overload.
        @return The result.
    */
    int
    operator()() const;
};

} // namespace impl

/** Public variable with implementation type in impl::. */
constexpr impl::hidden_fn visible_fo = {};

/** Forced function object with mixed public members.

    The type has operator(), a public data member, and a
    non-operator() function. The @functor command
    forces treatment: the type is hidden as
    implementation-defined, and only operator() overloads
    are exposed on the variable. The extra members (value
    and reset()) become invisible.
*/
struct mixed_members_fn
{
    /** The call operator.
        @param x The input.
        @return The result.
    */
    int
    operator()(int x) const;

    /** A public data member. */
    int value;

    /** A non-operator() public function. */
    void
    reset();
};

/** @functor
    Forced function object with extra hidden members.
*/
constexpr mixed_members_fn mixed = {};

/** Auto-detected: has all six explicit special member functions.

    Tests that special member detection correctly identifies
    destructor, default constructor, copy/move constructors,
    and copy/move assignment operators so they are skipped
    during auto-detection.
*/
struct all_special_fn
{
    all_special_fn() = default;
    ~all_special_fn() = default;
    all_special_fn(all_special_fn const&) = default;
    all_special_fn(all_special_fn&&) = default;
    all_special_fn& operator=(all_special_fn const&) = default;
    all_special_fn& operator=(all_special_fn&&) = default;

    /** All-special overload.
        @return The result.
    */
    int
    operator()() const;
};

constexpr all_special_fn all_special_fo = {};

/** Auto-detected: default constructor with all-defaulted
    parameters.
*/
struct default_ctor_with_all_defaults_fn
{
    constexpr default_ctor_with_all_defaults_fn(int = 0, int = 0) {}

    /** Defaulted-ctor overload.
        @return The result.
    */
    int
    operator()() const;
};

constexpr default_ctor_with_all_defaults_fn default_ctor_with_all_defaults_fo = {};

/** Auto-detected: copy assignment operator by value.

    The C++ standard allows operator=(X) by value as a
    copy assignment operator.
*/
struct by_value_assign_fn
{
    by_value_assign_fn& operator=(by_value_assign_fn);

    /** By-value overload.
        @return The result.
    */
    int
    operator()() const;
};

constexpr by_value_assign_fn by_value_assign_fo = {};

/** Auto-detected: template operator() in a non-template struct.

    This is the niebloid pattern used by range algorithms
    (e.g. std::ranges::sort). The struct itself is not a
    template, but operator() is.
*/
struct template_call_fn
{
    /** Sort a range.
        @param first The beginning of the range.
        @param last The end of the range.
    */
    template<class Iter>
    void
    operator()(Iter first, Iter last) const;
};

constexpr template_call_fn template_call_fo = {};

/** Not auto-detected: has a template constructor.

    A template constructor is not a special member function
    per the C++ standard, so it counts as a non-operator()
    public member that prevents auto-detection.
*/
struct template_ctor_fn
{
    template_ctor_fn() = default;

    template<class U>
    explicit template_ctor_fn(U);

    /** Template-ctor overload.
        @return The result.
    */
    int
    operator()() const;
};

constexpr template_ctor_fn template_ctor_fo = {};

/** Not auto-detected: has a template assignment operator.

    A template operator= is not a special member function
    per the C++ standard, so it counts as a non-operator()
    public member that prevents auto-detection.
*/
struct template_assign_fn
{
    template<class U>
    template_assign_fn& operator=(U);

    /** Template-assign overload.
        @return The result.
    */
    int
    operator()() const;
};

constexpr template_assign_fn template_assign_fo = {};

/** A class with a protected static function object member.
*/
struct host_protected
{
    struct invoke_fn
    {
        /** Invoke the operation.
            @return The result.
        */
        int
        operator()() const;
    };

protected:
    /** A protected static function object member. */
    static invoke_fn invoke;
};

/** A class with a private static function object member.
*/
struct host_private
{
    struct invoke_fn
    {
        /** Invoke the operation.
            @return The result.
        */
        int
        operator()() const;
    };

private:
    /** A private static function object member. */
    static invoke_fn invoke;
};

/** A function object type with a nested record inside.
    The nested record is private, so it doesn't prevent
    auto-detection.
*/
struct nested_record_fn
{
    /** Nested-record overload.
        @return The result.
    */
    int
    operator()() const;

private:
    struct inner {};
};

constexpr nested_record_fn nested_record_fo = {};

/** @functionobject
    Marked as a function object but the type has no operator().
    The finalizer should detect the empty overload set and
    leave the variable unchanged. Also, a warning should be
    emitted.
*/
struct no_call_fn
{
    void other() const;
};

constexpr no_call_fn no_call_fo = {};

/*  Completely undocumented function object: neither the type
    nor operator() nor the variable has a doc comment.
*/
struct undocumented_fo_fn
{
    void
    operator()() const;
};

constexpr undocumented_fo_fn undocumented_both = {};

/** @functionobject
    Command on the type, not the variable. The command
    should be recognized on the record's doc comment and
    apply to every variable of this type.
*/
struct marked_on_type_fn
{
    /** Overload from type-marked function object.
        @return The result.
    */
    int
    operator()() const;

    /** Extra member that prevents auto-detection. */
    void
    extra();
};

constexpr marked_on_type_fn marked_on_type = {};

/** Not auto-detected: type is a class template from a
    non-enclosing scope (external::), so the template check
    rejects it.
*/
constexpr external::hash_like<int> external_template = {};

} // namespace example
