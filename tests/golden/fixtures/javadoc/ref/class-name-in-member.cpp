// Within a class, the name of the class names the class, not its
// constructors, as in C++. Only after a qualifier naming the class does a
// name refer to the constructors, when it is the name of the class or
// repeats the qualifier: `Widget::Widget`, `WidgetAlias::Widget` and
// `WidgetAlias::WidgetAlias`. So a reference to the class from one of its
// members links to the class, as the same reference from outside it does.

/// A widget.
struct Widget
{
    /// Construct an empty widget.
    Widget();

    /// Resize it. See @ref Widget.
    void resize(int n);

    /// Reset it. See @ref Widget::Widget.
    void reset();
};

/// A gadget, with overloaded constructors.
struct Gadget
{
    /// Construct an empty gadget.
    Gadget();

    /// Construct a gadget of a given size.
    explicit Gadget(int n);

    /// Resize it. See @ref Gadget.
    void resize(int n);

    /// Reset it. See @ref Gadget::Gadget.
    void reset();
};

/// Use one. See @ref Widget.
void use(Widget& w);

/// Another name for @ref Widget.
using WidgetAlias = Widget;

/// Make one. See @ref WidgetAlias::Widget and @ref WidgetAlias::WidgetAlias.
Widget make();
