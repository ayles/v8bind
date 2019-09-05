//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_UTILITY_HPP
#define SANDWICH_V8B_UTILITY_HPP

#include <v8.h>

#include <tuple>

namespace v8b {
namespace utility {

template<typename F>
struct FunctionTraits;

// function
template<typename R, typename ...Args>
struct FunctionTraits<R(Args...)> {
    using ReturnType = R;
    using Arguments = std::tuple<Args...>;
    using PointerType = R (*)(Args...);
};

// function pointer
template<typename R, typename ...Args>
struct FunctionTraits<R (*)(Args...)> : FunctionTraits<R(Args...)> {
};

// function reference
template<typename R, typename ...Args>
struct FunctionTraits<R (&)(Args...)> : FunctionTraits<R(Args...)> {
};

// member function pointer
template<typename C, typename R, typename ...Args>
struct FunctionTraits<R (C::*)(Args...)> : FunctionTraits<R(C &, Args...)> {
    using ClassType = C;
    using PointerType = R (C::*)(Args...);
};

// member object pointer
template<typename C, typename R>
struct FunctionTraits<R (C::*)> {
    using ClassType = C;
    using PointerType = R (C::*);
    using ReturnType = R;
};

struct NonStrict {};

template<typename T, typename Enable = void>
struct ArgumentTraits;

template<>
struct ArgumentTraits<std::tuple<>, NonStrict> {
    template<typename R>
    static bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return true;
    }
};

template<>
struct ArgumentTraits<std::tuple<>> {
    template<typename R>
    static bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return info.Length() == 0;
    }
};

template<typename A>
struct ArgumentTraits<std::tuple<A>, NonStrict> {
    template<typename R>
    static bool IsMatch(const v8::FunctionCallbackInfo<R> &info, int offset = 0) {
        return Convert<std::decay_t<A>>::IsValid(info.GetIsolate(), info[offset]);
    }
};

template<typename A>
struct ArgumentTraits<std::tuple<A>> {
    template<typename R>
    static bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return info.Length() == 1 && ArgumentTraits<std::tuple<A>, NonStrict>::IsMatch(info);
    }
};

template<typename A1, typename A2, typename ...A>
struct ArgumentTraits<std::tuple<A1, A2, A...>, NonStrict> {
    template<typename R>
    static bool IsMatch(const v8::FunctionCallbackInfo<R> &info, int offset = 0) {
        return ArgumentTraits<std::tuple<A1>, NonStrict>::IsMatch(info, offset) &&
               ArgumentTraits<std::tuple<A2, A...>, NonStrict>::IsMatch(info, offset + 1);
    }
};

template<typename A1, typename A2, typename ...A>
struct ArgumentTraits<std::tuple<A1, A2, A...>> {
    template<typename R>
    static bool IsMatch(const v8::FunctionCallbackInfo<R> &info) {
        return std::tuple_size_v<std::tuple<A1, A2, A...>> == info.Length() &&
            ArgumentTraits<std::tuple<A1, A2, A...>, NonStrict>::IsMatch(info);
    }
};

template<typename T>
using NonStrictArgumentTraits = ArgumentTraits<T, NonStrict>;

}
}

#endif //SANDWICH_V8B_UTILITY_HPP
