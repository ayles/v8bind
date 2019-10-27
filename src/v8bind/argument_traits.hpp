//
// Created by selya on 27.10.2019.
//

#ifndef SANDWICH_V8B_ARGUMENT_TRAITS_HPP
#define SANDWICH_V8B_ARGUMENT_TRAITS_HPP

#include <v8bind/convert.hpp>

#include <v8.h>

#include <tuple>

namespace v8b::traits {

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

} //namespace v8b::traits

#endif //SANDWICH_V8B_ARGUMENT_TRAITS_HPP
