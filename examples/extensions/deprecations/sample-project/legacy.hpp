#include "vendor.hpp"

/// A small library that is partway through a rename.
namespace shapes {

/// The old name of the circle type.
struct [[deprecated("use Circle instead")]] Ring
{
    /// The radius of the ring.
    double radius;
};

/// A circle.
struct Circle
{
    /// The radius of the circle.
    double radius;

    /// The old way to get the radius.
    [[deprecated("use the radius member instead")]]
    double getRadius() const;

    /// The area of the circle.
    double area() const;

    [[deprecated("slated for removal")]]
    void resize(double factor);

    [[deprecated]]
    void reset();
};

/// The shapes the library can draw.
enum class Style
{
    /// Lines only.
    outline,
    /// Filled areas.
    filled,
    /// Superseded by filled.
    solid [[deprecated]]
};

/// Flags of an old interface, which have no name for the enumeration.
enum [[deprecated("use Style instead")]]
{
    oldOutline,
    oldFilled
};

/// Grows a shape, in whole or fractional steps.
[[deprecated("use Circle::resize instead")]]
void grow(int steps);

/// Grows a shape, in whole or fractional steps.
[[deprecated("use Circle::resize with a factor instead")]]
void grow(double steps);

/// The old name of the default style.
[[deprecated("use Style::filled instead")]]
extern Style defaultStyle;

} // namespace shapes

namespace shapes {

/// A circle with a label. It inherits a deprecated member of a type of
/// another library, which the report leaves to that library.
struct Label : vendor::Base
{
    /// Takes the old label type.
    void set(vendor::Old const& label);
};

} // namespace shapes

namespace shapes::detail {

/// A helper type that the documentation hides.
struct [[deprecated("internal")]] Helper
{
};

} // namespace shapes::detail

namespace shapes {

/// Draws with a helper.
void draw(detail::Helper const& helper);

} // namespace shapes

namespace shapes {

/// A box that holds a value.
template <class T>
struct Box
{
    /// Stores a value.
    void store(T const& value);
};

/// A box that holds an integer.
template <>
struct Box<int>
{
    [[deprecated("use Box<long> instead")]]
    void store(int value);
};

/// A box that holds a value of another library.
template <>
struct Box<vendor::Old>
{
    [[deprecated("use Box<int> instead")]]
    void store(vendor::Old const& value);
};

} // namespace shapes

namespace shapes {

[[deprecated("use Circle::resize instead")]]
void retire(int steps);

[[deprecated("use Circle::resize instead")]]
void retire(double steps);

[[deprecated("use Circle::resize with an int")]]
void drop(int steps);

[[deprecated("use Circle::resize with a double")]]
void drop(double steps);

} // namespace shapes
