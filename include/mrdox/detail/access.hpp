//
// This is a derivative work.
// Copyright Hubert Liberacki (hliberacki@gmail.com)
// Copyright Krzysztof Ostrowski
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_DETAIL_ACCESS_HPP
#define MRDOX_DETAIL_ACCESS_HPP

#include <functional>

namespace clang {
namespace mrdox {

// Original project: https://github.com/hliberacki/cpp-member-accessor

namespace access {

template<typename C, typename T>
struct MemberWrapper
{
    using type = T (C::*);
};

template<class C, class R, typename... Args>
struct FunctionWrapper
{
    using type = R (C::*)(Args...);
};

template<class C, class R, typename... Args>
struct ConstFunctionWrapper
{
    using type = R (C::*)(Args...) const;
};

template<class Tag, class T>
struct Proxy
{
    static typename T::type value;
};

template <class Tag, class T>
typename T::type Proxy<Tag, T>::value;

template<class T, typename T::type AccessPointer>
class MakeProxy
{
    struct Setter { Setter() { Proxy<T, T>::value = AccessPointer; } };
    static Setter instance;
};

template<class T, typename T::type AccessPointer>
typename MakeProxy<T, AccessPointer>::Setter MakeProxy<T, AccessPointer>::instance;

template<typename Sig, class Instance, typename... Args>
auto callFunction(Instance & instance, Args ...args)
{
    return (instance.*(Proxy<Sig, Sig>::value))(args...);
}

template<typename Sig, class Instance>
auto accessMember(Instance & instance)
{
    return std::ref(instance.*(Proxy<Sig, Sig>::value));
}

#define FUNCTION_HELPER(...)                                           \
  access::FunctionWrapper<__VA_ARGS__>

#define CONST_FUNCTION_HELPER(...)                                     \
  access::ConstFunctionWrapper<__VA_ARGS__>

#define FUNCTION_ACCESS(accessor_name, base, method, ...)            \
  struct accessor_name : FUNCTION_HELPER(base, __VA_ARGS__) {};            \
  template class access::MakeProxy<accessor_name, &base::method>;

#define CONST_FUNCTION_ACCESS(accessor_name, base, method, ...)      \
  struct accessor_name : CONST_FUNCTION_HELPER(base, __VA_ARGS__) {};      \
  template class access::MakeProxy<accessor_name, &base::method>;

#define MEMBER_ACCESS(accessor_name, base, member, ret_type)         \
  struct accessor_name : access::MemberWrapper<base, ret_type> {};       \
  template class access::MakeProxy<accessor_name, &base::member>;

} // access

} // mrdox
} // clang

#endif
