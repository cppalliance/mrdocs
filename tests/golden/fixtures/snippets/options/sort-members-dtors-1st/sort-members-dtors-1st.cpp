/// A class whose destructor sits at the top of the
/// member list.
struct widget
{
    /// Refresh the widget.
    void refresh();
    /// Construct an empty widget.
    widget();
    /// Destroy the widget and release resources.
    ~widget();
};
