//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_TRAITS_HPP
#define SANDWICH_V8B_TRAITS_HPP

#include <vector>
#include <tuple>

namespace v8b::traits {

template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template<typename T>
constexpr bool is_vector_v = is_vector<T>::value;

template<typename T>
struct tuple_tail;

template<typename Head, typename ...Tail>
struct tuple_tail<std::tuple<Head, Tail...>> {
    using type = std::tuple<Tail...>;
};

template<typename T>
using tuple_tail_t = typename tuple_tail<T>::type;

template<typename F>
struct function_traits;

// function
template<typename R, typename ...Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using arguments = std::tuple<Args...>;
    using pointer_type = R (*)(Args...);
};

// function pointer
template<typename R, typename ...Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)> {};

// function const pointer
template<typename R, typename ...Args>
struct function_traits<R (* const)(Args...)> : function_traits<R(Args...)> {
    using pointer_type = R (* const)(Args...);
};

// member function pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::*)(Args...)> : function_traits<R(C &, Args...)> {
    using class_type = C;
    using pointer_type = R (C::*)(Args...);
};

// member function const pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::* const)(Args...)> : function_traits<R(C &, Args...)> {
    using class_type = C;
    using pointer_type = R (C::* const)(Args...);
};

// const member function pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::*)(Args...) const> : function_traits<R (const C &, Args...)> {
    using pointer_type = R (C::*)(Args...) const;
};

// const member function const pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::* const)(Args...) const> : function_traits<R (const C &, Args...)> {
    using pointer_type = R (C::* const)(Args...) const;
};

#if defined(__cpp_noexcept_function_type) || __cplusplus >= 201703L

// function noexcept
template<typename R, typename ...Args>
struct function_traits<R(Args...) noexcept> {
    using return_type = R;
    using arguments = std::tuple<Args...>;
    using pointer_type = R (*)(Args...) noexcept;
};

// function noexcept pointer
template<typename R, typename ...Args>
struct function_traits<R (*)(Args...) noexcept> : function_traits<R(Args...) noexcept> {};

// function const noexcept pointer
template<typename R, typename ...Args>
struct function_traits<R (* const)(Args...) noexcept> : function_traits<R(Args...) noexcept> {
    using pointer_type = R (* const)(Args...) noexcept;
};

// member function noexcept pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::*)(Args...) noexcept> : function_traits<R(C &, Args...) noexcept> {
    using class_type = C;
    using pointer_type = R (C::*)(Args...) noexcept;
};

// member function const pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::* const)(Args...) noexcept> : function_traits<R(C &, Args...) noexcept> {
    using class_type = C;
    using pointer_type = R (C::* const)(Args...) noexcept;
};


// const member function noexcept pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::*)(Args...) const noexcept> : function_traits<R (const C &, Args...) noexcept> {
    using pointer_type = R (C::*)(Args...) const noexcept;
};

// const member function const noexcept pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::* const)(Args...) const noexcept> : function_traits<R (const C &, Args...) noexcept> {
    using pointer_type = R (C::* const)(Args...) const noexcept;
};

#endif

// function object, lambda
template<typename F>
struct function_traits {
private:
    using callable_traits = function_traits<decltype(&F::operator())>;
public:
    using return_type = typename callable_traits::return_type;
    using arguments = tuple_tail_t<typename callable_traits::arguments>;
    using pointer_type = typename callable_traits::pointer_type;
};

// member object pointer
template<typename C, typename R>
struct function_traits<R (C::*)> {
    using class_type = C;
    using pointer_type = R (C::*);
    using return_type = R;
};

template<typename F>
struct function_traits<F&> : function_traits<F> {};

template<typename F>
struct function_traits<F&&> : function_traits<F> {};

template<typename ...Args>
constexpr bool multi_and(Args &&... a) {
    return (a && ...);
}

template<typename ...Args>
constexpr bool multi_or(Args &&... a) {
    return (a || ...);
}

}

#endif //SANDWICH_V8B_TRAITS_HPP
