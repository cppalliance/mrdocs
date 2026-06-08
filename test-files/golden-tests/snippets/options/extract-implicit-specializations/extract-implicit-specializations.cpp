/// A simple container template.
template <class T>
struct box
{
    /// The contained value.
    T value;
};

/// Uses `box<int>`. With
/// `extract-implicit-specializations: true`, that
/// implicit instantiation shows up in the docs as
/// `box<int>` alongside the primary template.
box<int> make_int_box(int v);
