/// A pointer to function typedef
using fun_ptr = void (*)();

/// This struct will be inherited as public
struct X
{
    /// Conversion operator to function pointer.
    operator fun_ptr() const;
};

/// This struct inherits from X
struct Y : X
{
    /// Bring X::operator fun_ptr into Y.
    using X::operator fun_ptr;
};
