// A member belongs to the documented surface only if its enclosing record
// does. The members below have public access inside private nested classes,
// or are defined out of line at namespace scope where the record is not in
// sight; none of them reaches the output, so none is reported as
// undocumented. Only `undocumented_public` is expected to be flagged.

/// A documented class.
class Outer {
    /// A documented private struct.
    struct Hidden {
        int public_member_of_private_struct;
        void public_method_of_private_struct();
    };

    /// A private class template whose static member is defined out of line.
    template <typename T> struct TypeId { static char Id; };

    class Concept;

public:
    void undocumented_public();

    /// Documented.
    void documented_public();
};

template <typename T> char Outer::TypeId<T>::Id = 1;

/// A documented private class, defined out of line.
class Outer::Concept {
public:
    virtual ~Concept() = 0;
    virtual void alias(int a) = 0;
};
