// NOTE: This golden test enables tagfile generation to verify that types nested
// inside a class (not just inside a namespace) get their own tagfile entry.
// See https://github.com/cppalliance/mrdocs/issues/1232.

namespace ns {

/// A class containing a nested type.
struct outer
{
    /// A struct nested in a class: it must get its own tagfile compound.
    struct config
    {
        void apply();
    };

    /// An enum nested in a class.
    enum class mode { a, b };

    void method();
};

/// A type nested in a namespace (control: already worked before the fix).
struct response_factory
{
    int make();
};

} // namespace ns
