/// A class declared with members in deliberately
/// scattered order to show how sorting affects the
/// rendered member list.
struct gadget
{
    /// Refresh the gadget.
    void refresh();
    /// Construct a gadget.
    gadget();
    /// Hide the gadget.
    void hide();
    /// Show the gadget.
    void show();
    /// Destroy the gadget.
    ~gadget();
};
