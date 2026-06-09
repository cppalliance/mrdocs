#include <type_traits>

// Regression test for a crash extracting a class template whose partial
// specializations each constrain a different SFINAE control parameter
// (En1 in one specialization, En2 in the other). This is the exact snippet
// from issue #823.

namespace boost::mysql {

template <class T, class En1 = void, class En2 = void>
struct writable_field_traits
{
};

template <class T>
struct writable_field_traits<T, typename std::enable_if<std::is_same<T, int>::value>::type, void>
{
};

template <class T>
struct writable_field_traits<T, void, typename std::enable_if<std::is_same<T, bool>::value>::type>
{
};

}  // namespace boost::mysql
