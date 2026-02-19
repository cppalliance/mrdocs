/// A class template that befriends a template parameter.
template <class T>
class Container
{
    /// T is a type, not a resolvable symbol ID.
    friend T;

    int secret_;
};
