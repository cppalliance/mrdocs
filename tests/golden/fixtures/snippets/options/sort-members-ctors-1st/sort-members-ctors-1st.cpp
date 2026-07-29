/// A class whose constructors should head the
/// rendered member list regardless of where the
/// declarations sit in the source.
struct widget
{
    /// Hide the widget.
    void hide();
    /// Show the widget.
    void show();
    /// Construct an empty widget.
    widget();
    /// Construct a widget with the given label.
    widget(const char* label);
};
