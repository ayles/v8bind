//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_FUNCTION_HPP
#define SANDWICH_V8B_FUNCTION_HPP

#include <v8bind/class.hpp>
#include <v8bind/convert.hpp>
#include <v8bind/traits.hpp>

#include <v8.h>

#include <type_traits>
#include <cstring>
#include <functional>
#include <utility>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <memory>

namespace v8b {

// Class wrapping copy of any data in v8::Local
// Lifetime of such object is controlled by V8 GC
class ExternalData {
public:
    // Any primitive that will fit in sizeof(void *) will be just copied
    // Otherwise object will be copied to container with lifetime bound to Weak Persistent handle
    template<typename T>
    static constexpr bool is_bitcast_allowed =
            sizeof(T) <= sizeof(void*) &&
            std::is_default_constructible_v<T> &&
            std::is_trivially_copyable_v<T>;

    template<typename T>
    static v8::Local<v8::Value> New(v8::Isolate* isolate, T &&data) {
        if constexpr (is_bitcast_allowed<T>) {
            void *ptr;
            std::memcpy(&ptr, &data, sizeof(data));
            return v8::External::New(isolate, ptr);
        } else {
            return DataHolder<T>::New(isolate, std::forward<T>(data));
        }
    }

    template<typename T>
    static decltype(auto) Unwrap(v8::Local<v8::Value> value) {
        if constexpr (is_bitcast_allowed<T>) {
            void *ptr = value.As<v8::External>()->Value();
            T data;
            std::memcpy(&data, &ptr, sizeof(data));
            return data;
        } else {
            auto data_holder = static_cast<DataHolder<T> *>(value.As<v8::External>()->Value());
            return data_holder->Get();
        }
    }

private:
    template<typename T>
    struct DataHolder {
        T &Get() {
            return *static_cast<T *>(static_cast<void *>(&storage));
        }

        static v8::Local<v8::Value> New(v8::Isolate *isolate, T &&data) {
            auto data_holder = new DataHolder<T>;
            new (data_holder->GetData()) T(std::forward<T>(data));
            auto external = v8::External::New(isolate, data_holder);
            data_holder->handle.Reset(isolate, external);
            data_holder->handle.SetWeak(data_holder, [](const v8::WeakCallbackInfo<DataHolder<T>> &data) {
                std::unique_ptr<DataHolder<T>> d_h(data.GetParameter());
                if (!d_h->handle.IsEmpty()) {
                    d_h->GetData()->~T();
                    d_h->handle.Reset();
                }
            }, v8::WeakCallbackType::kParameter);
            return external;
        }

    private:
        std::aligned_storage_t<sizeof(T), alignof(T)> storage;
        v8::Global<v8::External> handle;

        T *GetData() {
            return static_cast<T *>(static_cast<void *>(&storage));
        }
    };
};

struct MemberCall {};
struct StaticCall {};

namespace impl {

template<typename CallType, bool wrap_return_value, typename F, size_t ...Indices>
decltype(auto) CallNativeFromV8Impl(F &&f, const v8::FunctionCallbackInfo<v8::Value> &info,
                          std::index_sequence<Indices...>) {
    using Arguments = typename traits::function_traits<F>::arguments;
    using ReturnType = typename traits::function_traits<F>::return_type;

    if constexpr (std::is_same_v<CallType, StaticCall>) {
        if constexpr (std::tuple_size_v<Arguments> == 1) {
            if constexpr (
                    std::is_same_v<std::tuple_element_t<0, Arguments>, const v8::FunctionCallbackInfo<v8::Value> &>) {
                return std::invoke(std::forward<F>(f), info);
            }
        }

        if constexpr (std::is_same_v<ReturnType, void>) {
            return std::invoke(std::forward<F>(f),
                        FromV8<std::tuple_element_t<Indices, Arguments>>(info.GetIsolate(), info[Indices])...);
        } else {
            auto result = std::invoke(std::forward<F>(f),
                                      FromV8<std::tuple_element_t<Indices, Arguments>>(
                                              info.GetIsolate(), info[Indices])...);
            if constexpr (wrap_return_value) {
                info.GetReturnValue().Set(ToV8(info.GetIsolate(), result));
            }
            return result;
        }
    } else if (std::is_same_v<CallType, MemberCall>) {
        auto object = FromV8<std::tuple_element_t<0, Arguments>>(info.GetIsolate(), info.This());

        if constexpr (std::tuple_size_v<Arguments> == 2) {
            if constexpr (
                    std::is_same_v<std::tuple_element_t<1, Arguments>, const v8::FunctionCallbackInfo<v8::Value> &>) {
                return std::invoke(std::forward<F>(f), object, info);
            }
        }

        if constexpr (std::is_same_v<ReturnType, void>) {
            return std::invoke(std::forward<F>(f), object,
                        FromV8<std::tuple_element_t<Indices + 1, Arguments>>(info.GetIsolate(), info[Indices])...);
        } else {
            auto result = std::invoke(std::forward<F>(f), object,
                                      FromV8<std::tuple_element_t<Indices + 1, Arguments>>(
                                              info.GetIsolate(), info[Indices])...);
            if constexpr (wrap_return_value) {
                info.GetReturnValue().Set(ToV8(info.GetIsolate(), result));
            }
            return result;
        }
    }
}

} // namespace impl

// Call any C++ function with arguments conversion from V8
// Returned value will be converted back to V8 and passed to info return value
// Also return value will be returned as result of executing this function
// allowing use this in factory functions
// Use MemberCall or StaticCall as CallType value to indicate either
// member call ("this" object unwrapped and passed as first argument) or
// static call (functions just called without "this" reference)
// Set wrap_return_value to false if you using this to return value to C++
template<typename CallType, bool wrap_return_value = true, typename F>
decltype(auto) CallNativeFromV8(F &&f, const v8::FunctionCallbackInfo<v8::Value> &info) {
    static_assert(std::is_same_v<CallType, MemberCall> || std::is_same_v<CallType, StaticCall>,
                  "CallType must be either MemberCall or StaticCall");

    using Arguments = typename traits::function_traits<F>::arguments;

    if constexpr (std::is_same_v<CallType, StaticCall>) {
        if (!traits::argument_traits<Arguments>::is_match(info)) {
            throw std::runtime_error("Arguments don't match");
        }
        using Indices = std::make_index_sequence<std::tuple_size_v<Arguments>>;
        return impl::CallNativeFromV8Impl<CallType, wrap_return_value>(std::forward<F>(f), info, Indices {});
    } else if (std::is_same_v<CallType, MemberCall>) {
        if (!traits::argument_traits<traits::tuple_tail_t<Arguments>>::is_match(info)) {
            throw std::runtime_error("Arguments don't match");
        }
        using Indices = std::make_index_sequence<std::tuple_size_v<Arguments> - 1>;
        return impl::CallNativeFromV8Impl<CallType, wrap_return_value>(std::forward<F>(f), info, Indices {});
    }
}

// Try sequentially call functions from functions tuple
// and stop on first successful call
template<typename CallType, size_t index = 0, typename ...FS>
void SelectAndCall(
        const v8::FunctionCallbackInfo<v8::Value> &info,
        const std::tuple<FS...> &functions) {
    if constexpr (index < std::tuple_size_v<std::tuple<FS...>>) {
        try {
            CallNativeFromV8<CallType>(std::get<index>(functions), info);
        } catch (const std::exception &) {
            SelectAndCall<CallType, index + 1>(info, functions);
        }
    } else {
        throw std::runtime_error("No suitable function found to call");
    }
}

// Wrap functions as overloads of one js function
// Use MemberCall or StaticCall as CallType value to indicate either
// member call ("this" object unwrapped and passed as first argument) or
// static call (functions just called without "this" reference)
template<typename CallType, typename ...F>
v8::Local<v8::FunctionTemplate> WrapFunction(v8::Isolate *isolate, F&&... f) {
    static_assert(std::is_same_v<CallType, MemberCall> || std::is_same_v<CallType, StaticCall>,
                  "CallType must be either MemberCall or StaticCall");

    v8::EscapableHandleScope scope(isolate);

    std::tuple functions(std::forward<F>(f)...);

    return scope.Escape(v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
        try {
            auto &extracted_functions = ExternalData::Unwrap<decltype(functions)>(info.Data());
            SelectAndCall<CallType>(info, extracted_functions);
        } catch (const std::exception &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), e.what())));
        }
    }, ExternalData::New(isolate, std::move(functions))));
}

template<size_t index = 0, typename ...FS>
decltype(auto) SelectAndCallConstructor(
        const v8::FunctionCallbackInfo<v8::Value> &args,
        const std::tuple<FS...> &constructors) {
    if constexpr (index < std::tuple_size_v<std::tuple<FS...>>) {
        try {
            return CallNativeFromV8<StaticCall, false>(std::get<index>(constructors), args);
        } catch (const std::exception &e) {
            return SelectAndCallConstructor<index + 1>(args, constructors);
        }
    } else {
        throw std::runtime_error("No suitable constructor found to call");
    }
}

// Wrap passed functions as overloaded constructors
template<typename ...F>
void WrapConstructor(v8::Isolate *isolate, F&&... f, v8::Local<v8::FunctionTemplate> t) {
    v8::HandleScope scope(isolate);

    std::tuple constructors(std::forward<F>(f)...);

    t->SetCallHandler([](const v8::FunctionCallbackInfo<v8::Value> &args) {
        try {
            auto &extracted_constructors = ExternalData::Unwrap<decltype(constructors)>(args.Data());
            auto o = SelectAndCallConstructor(args, extracted_constructors);
            args.GetReturnValue().Set(Class<std::remove_pointer_t<decltype(o)>>::WrapObject(args.GetIsolate(), o, true));
        } catch (const std::exception &e) {
            args.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(args.GetIsolate(), e.what())));
        }
    }, ExternalData::New(isolate, std::move(constructors)));
}

namespace impl {

template<typename T, typename AS, size_t ...Indices>
T *CallConstructorImpl(const v8::FunctionCallbackInfo<v8::Value> &info,
                       std::index_sequence<Indices...>) {
    return new T(FromV8<std::tuple_element_t<Indices, AS>>(
            info.GetIsolate(), info[Indices])...);
}

} // namespace impl

template<typename T, typename AS>
T *CallConstructor(const v8::FunctionCallbackInfo<v8::Value> &args) {
    if (!traits::argument_traits<AS>::is_match(args)) {
        throw std::runtime_error("No suitable constructor found");
    }
    using indices = std::make_index_sequence<std::tuple_size_v<AS>>;
    return impl::CallConstructorImpl<T, AS>(args, indices {});
}

template<typename T, typename AS1, typename AS2, typename ...Args>
T *CallConstructor(const v8::FunctionCallbackInfo<v8::Value> &args) {
    try {
        return CallConstructor<T, AS1>(args);
    } catch (const std::exception &) {}
    return CallConstructor<T, AS2, Args...>(args);
}

}

#endif //SANDWICH_V8B_FUNCTION_HPP
