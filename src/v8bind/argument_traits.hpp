//
// Created by selya on 27.10.2019.
//

#ifndef SANDWICH_V8B_ARGUMENT_TRAITS_HPP
#define SANDWICH_V8B_ARGUMENT_TRAITS_HPP

#include <v8bind/convert.hpp>

#include <v8.h>

#include <tuple>

namespace v8b::traits {

struct NonStrict {};

template<typename T, typename Enable = void>
struct ArgumentTraits;

template<>
struct ArgumentTraits<std::tuple<>, NonStrict> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return true;
    }
};

template<>
struct ArgumentTraits<std::tuple<>> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return info.Length() == 0;
    }
};

// Allow bind functions with native v8 signature
template<typename IR>
struct ArgumentTraits<std::tuple<const v8::FunctionCallbackInfo<IR> &>, NonStrict> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return std::is_same_v<IR, R>;
    }
};

template<typename IR>
struct ArgumentTraits<std::tuple<const v8::FunctionCallbackInfo<IR> &>> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return std::is_same_v<IR, R>;
    }
};

template<typename A>
struct ArgumentTraits<std::tuple<A>, NonStrict> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info, int offset = 0) {
        return Convert<std::decay_t<A>>::IsValid(info.GetIsolate(), info[offset]);
    }
};

template<typename A>
struct ArgumentTraits<std::tuple<A>> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return info.Length() == 1 && ArgumentTraits<std::tuple<A>, NonStrict>::IsMatch(info);
    }
};

template<typename A1, typename A2, typename ...A>
struct ArgumentTraits<std::tuple<A1, A2, A...>, NonStrict> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info, int offset = 0) {
        return ArgumentTraits<std::tuple<A1>, NonStrict>::IsMatch(info, offset) &&
            ArgumentTraits<std::tuple<A2, A...>, NonStrict>::IsMatch(info, offset + 1);
    }
};

template<typename A1, typename A2, typename ...A>
struct ArgumentTraits<std::tuple<A1, A2, A...>> {
    template<typename R>
    static inline bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return std::tuple_size_v<std::tuple<A1, A2, A...>> == info.Length() &&
            ArgumentTraits<std::tuple<A1, A2, A...>, NonStrict>::IsMatch(info);
    }
};

template<typename T>
using NonStrictArgumentTraits = ArgumentTraits<T, NonStrict>;

} //namespace v8b::traits

#endif //SANDWICH_V8B_ARGUMENT_TRAITS_HPP
