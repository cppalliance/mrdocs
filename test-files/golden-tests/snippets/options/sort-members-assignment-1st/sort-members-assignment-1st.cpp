/// A class whose assignment operators should be
/// grouped at the top of the member list.
struct widget
{
    /// Refresh the widget.
    void refresh();
    /// Copy-assign from another widget.
    widget& operator=(const widget&);
    /// Move-assign from another widget.
    widget& operator=(widget&&);
};
