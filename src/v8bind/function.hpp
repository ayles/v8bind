//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_FUNCTION_HPP
#define SANDWICH_V8B_FUNCTION_HPP

#include <v8bind/convert.hpp>
#include <v8bind/traits.hpp>

#include <v8.h>

#include <type_traits>
#include <cstring>
#include <functional>
#include <utility>

namespace v8b {
namespace impl {

class ExternalData {
public:
    template<typename T>
    static constexpr bool is_bitcast_allowed =
            sizeof(T) <= sizeof(void*) &&
            std::is_default_constructible_v<T> &&
            std::is_trivially_copyable_v<T>;

    template<typename T>
    static v8::Local<v8::Value> New(v8::Isolate* isolate, T &&value) {
        if constexpr (is_bitcast_allowed<T>) {
            void *ptr;
            std::memcpy(&ptr, &value, sizeof(value));
            return v8::External::New(isolate, ptr);
        } else {
            auto data_holder = new DataHolder<T>;
            new (data_holder->GetData()) T(std::forward<T>(value));
            data_holder->handle.Reset(isolate, v8::External::New(isolate, data_holder));
            data_holder->handle.SetWeak(data_holder, [](const v8::WeakCallbackInfo<DataHolder<T>> &data) {
                auto data_holder = data.GetParameter();
                if (!data_holder->handle.IsEmpty()) {
                    data_holder->GetData()->~T();
                    data_holder->handle.Reset();
                }
            }, v8::WeakCallbackType::kParameter);
            return data_holder->handle.Get(isolate);
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
        std::aligned_storage_t<sizeof(T), alignof(T)> storage;
        v8::Global<v8::External> handle;

        T &Get() {
            return *static_cast<T *>(static_cast<void *>(&storage));
        }

        T *GetData() {
            return static_cast<T *>(static_cast<void *>(&storage));
        }
    };
};


template<typename R, typename Args, typename F, typename ...C, size_t ...Indices>
v8::Local<v8::Value> CallFunctionImpl(
        F &&callable,
        const v8::FunctionCallbackInfo<v8::Value> &info,
        std::index_sequence<Indices...>,
        C&&... o) {

    if constexpr (std::is_same_v<R, void>) {
        std::invoke(std::forward<F>(callable), std::forward<C>(o)...,
                    FromV8<std::tuple_element_t<Indices, Args>>(info.GetIsolate(), info[Indices])...);
        return v8::Local<v8::Value>();
    } else {
        return ToV8(info.GetIsolate(), std::invoke(std::forward<F>(callable), std::forward<C>(o)...,
                                                   FromV8<std::tuple_element_t<Indices, Args>>(info.GetIsolate(), info[Indices])...));
    }
}

template<typename R, typename Args, typename F, typename ...C>
v8::Local<v8::Value> CallFunction(
        F &&callable,
        const v8::FunctionCallbackInfo<v8::Value> &info,
        C&&... o) {
    using indices = std::make_index_sequence<std::tuple_size_v<Args>>;
    return CallFunctionImpl<R, Args, F, C...>(std::forward<F>(callable), info, indices {}, std::forward<C>(o)...);
}

template<typename R, typename Args, typename O = void, typename F>
decltype(auto) WrapFunctionImpl(F &&callable) {
    return [callable](const v8::FunctionCallbackInfo<v8::Value> &info) {
        if (!traits::argument_traits<Args>::is_match(info)) {
            throw std::runtime_error("No suitable function found");
        }
        if constexpr (std::is_same_v<O, void>) {
            info.GetReturnValue().Set(CallFunction<R, Args>(callable, info));
        } else {
            auto &object = FromV8<O>(info.GetIsolate(), info.This());
            info.GetReturnValue().Set(CallFunction<R, Args>(callable, info, object));
        }
    };
}

template<size_t index = 0, typename ...WFS>
void CallWrappedPack(
        const v8::FunctionCallbackInfo<v8::Value> &info,
        const std::tuple<WFS...> &wrapped_functions) {
    if constexpr (index < std::tuple_size_v<std::tuple<WFS...>>) {
        try {
            std::get<index>(wrapped_functions)(info);
        } catch (const std::exception &) {
            CallWrappedPack<index + 1>(info, wrapped_functions);
        }
    } else {
        throw std::runtime_error("No suitable function found");
    }
}

template<typename F>
decltype(auto) CallableFromFunction(F &&f) {
    if constexpr (std::is_member_function_pointer_v<F>) {
        return std::mem_fn(std::forward<F>(f));
    } else {
        return std::function(std::forward<F>(f));
    }
}

}

template<typename ...FS>
v8::Local<v8::FunctionTemplate> WrapFunction(v8::Isolate *isolate, FS&&... fs) {
    v8::EscapableHandleScope scope(isolate);

    std::tuple wrapped_functions(impl::WrapFunctionImpl<
            typename traits::function_traits<FS>::return_type,
            typename traits::function_traits<FS>::arguments>(
                    impl::CallableFromFunction(std::forward<FS>(fs)))...);

    return scope.Escape(v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
        try {
            auto &w_f = impl::ExternalData::Unwrap<decltype(wrapped_functions)>(info.Data());
            CallWrappedPack(info, w_f);
        } catch (const std::exception &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), std::string(e.what()))));
        }
    }, impl::ExternalData::New(isolate, std::move(wrapped_functions))));
}

template<typename ...FS>
v8::Local<v8::FunctionTemplate> WrapMemberFunction(v8::Isolate *isolate, FS&&... fs) {
    v8::EscapableHandleScope scope(isolate);

    std::tuple wrapped_functions(impl::WrapFunctionImpl<
            typename traits::function_traits<FS>::return_type,
            typename traits::tuple_tail_t<typename traits::function_traits<FS>::arguments>,
            typename std::tuple_element_t<0, typename traits::function_traits<FS>::arguments>>(
                impl::CallableFromFunction(std::forward<FS>(fs)))...);

    return scope.Escape(v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
        try {
            auto &w_f = impl::ExternalData::Unwrap<decltype(wrapped_functions)>(info.Data());
            CallWrappedPack(info, w_f);
        } catch (const std::exception &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), std::string(e.what()))));
        }
    }, impl::ExternalData::New(isolate, std::move(wrapped_functions))));
}

namespace impl {

template<typename T, typename AS, size_t ...Indices>
T *CallConstructorImpl(const v8::FunctionCallbackInfo<v8::Value> &info,
        std::index_sequence<Indices...>) {
    return new T(FromV8<std::tuple_element_t<Indices, AS>>(
                    info.GetIsolate(), info[Indices])...);
}

}

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
