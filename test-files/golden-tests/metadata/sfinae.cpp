#include <type_traits>
#include <stdexcept>

/// Enabled via return type
template <class T>
std::enable_if_t<std::is_integral_v<T>, T>
f1(T value);

/// Enabling a specified return type
template <class T>
std::enable_if_t<std::is_integral_v<T>, int>
f2(T value);

namespace B {
    struct C {};
}

/// Enabling a specified return type in another namespace
template <class T>
std::enable_if_t<std::is_integral_v<T>, B::C>
f3(T value);

/// Enabled via return type with std::enable_if
template <class T>
typename std::enable_if<std::is_integral_v<T>, T>::type
f4(T value);

/// Enabled via a non-type template parameter with helper
template <class T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
T
f5(T value);

/// Enabled via a non-type template parameter without helper
template<class T, typename std::enable_if<std::is_integral_v<T>, bool>::type = true>
T
f6(T value);

/// Enabled via a non-type template parameter using int instead of bool
template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void f7(T value);

/// Enabled via parameter without helper
template <class T>
T
f8(T value, typename std::enable_if<std::is_integral_v<T>>::type* = 0);

/// Enabled via parameter with helper
template <class T>
T
f9(T value, std::enable_if_t<std::is_integral_v<T>>* = 0);

/// Enabled via type template parameter
///
/// This pattern should not be used because the function signature
/// is unmodified and therefore only supports one overload.
///
/// It's a common mistake is to declare two function templates
/// that differ only in their default template arguments.
///
/// This does not work because the declarations are treated as
/// redeclarations of the same function template (default template
/// arguments are not accounted for in function template equivalence).
///
template<class T, typename = std::enable_if_t<std::is_integral_v<T>>>
void f10(T value);

/// The partial specialization of A is enabled via a template parameter
template<class T, class Enable = void>
class A {};

/// Specialization for floating point types
template<class T>
class A<T, std::enable_if_t<std::is_integral_v<T>>> {};

/// SFINAE with std::void_t
template <class T, class Enable = void>
struct S
{
    void store(void const*) {}
};

/// SFINAE with std::void_t
template <class T>
struct S<T, std::void_t<typename T::a::b>>
{
    void store(void const*) {}
};
