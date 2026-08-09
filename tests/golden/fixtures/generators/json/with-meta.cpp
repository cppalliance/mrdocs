namespace app {

/// A color.
enum class Color
{
    red,
    green,
    blue
};

/// A widget.
struct Widget
{
    /// The current value.
    int value() const;

    /// The color.
    Color color;
};

/// Make a widget.
Widget make();

} // namespace app
