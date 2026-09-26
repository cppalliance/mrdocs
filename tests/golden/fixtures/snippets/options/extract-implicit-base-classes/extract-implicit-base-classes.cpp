/// A simple container template.
template <class T>
struct box
{
    /// The contained value.
    T value;
};

/// Inherits from `box<int>`. With
/// `extract-implicit-base-classes: true`, that implicit
/// instantiation shows up in the docs alongside the
/// primary template.
struct int_box : box<int>
{
};
