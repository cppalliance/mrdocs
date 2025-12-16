
template <bool C, typename T>
struct enable_if {
    using type = T;
};

template <typename T>
struct enable_if<false, T> {};

template <typename T>
struct is_match {
    enum {
        value = false
    };
};

template <typename _Yp, typename _Del, typename _Res>
using _UniqCompatible = typename enable_if<is_match<_Yp>::value, _Res>::type;

template <typename _Yp, typename _Del>
using _UniqAssignable = _UniqCompatible<_Yp, _Del, int>;
