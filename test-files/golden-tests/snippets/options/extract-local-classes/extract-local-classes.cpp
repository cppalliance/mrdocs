/// A function whose local helper class is part of
/// the public interface (its instances escape via
/// the returned visitor).
inline void each()
{
    /// Local visitor type.
    struct visitor
    {
        void operator()(int) {}
    };
    visitor{}(0);
}
