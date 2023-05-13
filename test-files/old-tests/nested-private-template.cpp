template<class T>
class range
{
    template<class R, bool>
    struct impl;
};

template<class T>
template<class R>
struct range<T>::impl<R, false>
{
};
