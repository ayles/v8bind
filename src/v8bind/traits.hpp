//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_UTILITY_HPP
#define SANDWICH_V8B_UTILITY_HPP

#include <v8.h>

#include <tuple>

namespace v8b::traits {

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




struct non_strict {};

template<typename T, typename Enable = void>
struct argument_traits;

template<>
struct argument_traits<std::tuple<>, non_strict> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info) {
        return true;
    }
};

template<>
struct argument_traits<std::tuple<>> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info) {
        return info.Length() == 0;
    }
};

// Allow bind functions with native v8 signature
template<typename IR>
struct argument_traits<std::tuple<const v8::FunctionCallbackInfo<IR> &>, non_strict> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info) {
        return std::is_same_v<IR, R>;
    }
};

template<typename IR>
struct argument_traits<std::tuple<const v8::FunctionCallbackInfo<IR> &>> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info) {
        return std::is_same_v<IR, R>;
    }
};

template<typename A>
struct argument_traits<std::tuple<A>, non_strict> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info, int offset = 0) {
        return Convert<std::decay_t<A>>::IsValid(info.GetIsolate(), info[offset]);
    }
};

template<typename A>
struct argument_traits<std::tuple<A>> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info) {
        return info.Length() == 1 && argument_traits<std::tuple<A>, non_strict>::is_match(info);
    }
};

template<typename A1, typename A2, typename ...A>
struct argument_traits<std::tuple<A1, A2, A...>, non_strict> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info, int offset = 0) {
        return argument_traits<std::tuple<A1>, non_strict>::is_match(info, offset) &&
                argument_traits<std::tuple<A2, A...>, non_strict>::is_match(info, offset + 1);
    }
};

template<typename A1, typename A2, typename ...A>
struct argument_traits<std::tuple<A1, A2, A...>> {
    template<typename R>
    static bool is_match(const v8::FunctionCallbackInfo<R> &info) {
        return std::tuple_size_v<std::tuple<A1, A2, A...>> == info.Length() &&
                argument_traits<std::tuple<A1, A2, A...>, non_strict>::is_match(info);
    }
};

template<typename T>
using non_strict_argument_traits = argument_traits<T, non_strict>;


template<typename ...Args>
constexpr bool multi_and(Args &&... a) {
    return (a && ...);
}

template<typename ...Args>
constexpr bool multi_or(Args &&... a) {
    return (a || ...);
}

}

#endif //SANDWICH_V8B_UTILITY_HPP
