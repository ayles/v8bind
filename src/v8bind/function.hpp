//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_FUNCTION_HPP
#define SANDWICH_V8B_FUNCTION_HPP

#include <v8bind/convert.hpp>
#include <v8bind/utility.hpp>

#include <v8.h>

#include <type_traits>
#include <cstring>
#include <functional>

namespace v8b {

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

/*template<typename F, size_t ...Indices>
void call_f_impl(void *f_ptr, const v8::FunctionCallbackInfo<v8::Value> &info, std::index_sequence<Indices...>) {
    using pointer_type = typename function_traits<F>::pointer_type;
    using arguments_tuple_type = typename function_traits<F>::arguments;

    auto function_pointer = static_cast<pointer_type>(f_ptr);

    if constexpr (std::is_member_function_pointer_v<F>) {
        // TODO
    } else {
        function_pointer((
                convert<std::decay_t<std::tuple_element_t<Indices, arguments_tuple_type>>>
                ::from_v8(info.GetIsolate(), info[Indices]))...);
    }
}

template<typename F>
bool call_f(void **f_ptr, const v8::FunctionCallbackInfo<v8::Value> &info, int offset = 0) {
    if (!arguments_traits<typename function_traits<F>::arguments>::is_match(info)) {
        return false;
    }
    using indices = std::make_index_sequence<std::tuple_size_v<typename function_traits<F>::arguments>>;
    call_f_impl<F>(f_ptr[offset], info, indices {});
    return true;
}

template<typename F1, typename F2, typename ...F>
bool call_f(void **f_ptr, const v8::FunctionCallbackInfo<v8::Value> &info, int offset = 0) {
    return call_f<F1>(f_ptr, info, offset) || call_f<F2, F...>(f_ptr, info, offset + 1);
}

template<typename ...F>
decltype(auto) get_f() {
    return [](const v8::FunctionCallbackInfo<v8::Value> &info) {
        call_f<F...>(static_cast<void **>(info.Data().As<v8::External>()->Value()), info);
    };
}

template<typename ...F>
v8::Local<v8::FunctionTemplate> wrap_function(v8::Isolate *isolate, F&&... fs) {
    auto data = new std::vector<void *> { (static_cast<void *>(fs))... };
    return v8::FunctionTemplate::New(isolate, get_f<F...>(), v8::External::New(isolate, static_cast<void *>(data->data())));
}*/



template<typename F, typename ...C, size_t ...Indices>
v8::Local<v8::Value> CallFunctionImpl(F &&f,
        const v8::FunctionCallbackInfo<v8::Value> &info,
        std::index_sequence<Indices...>, C&&... o) {
    using Args = typename utility::FunctionTraits<F>::Arguments;
    return ToV8(info.GetIsolate(), std::invoke(std::forward<F>(f), std::forward<C>(o)...,
            FromV8<std::tuple_element_t<Indices, Args>>(info.GetIsolate(), info[Indices])...));
}

template<typename F, typename ...C>
v8::Local<v8::Value> CallFunction(F &&f,
        const v8::FunctionCallbackInfo<v8::Value> &info, C&&... o) {
    using Args = typename utility::FunctionTraits<F>::Arguments;
    using indices = std::make_index_sequence<std::tuple_size_v<Args>>;
    return CallFunctionImpl<F, C...>(std::forward<F>(f), info, indices {}, std::forward<C>(o)...);
}

template<typename F>
int SelectFunction(const v8::FunctionCallbackInfo<v8::Value> &info, int offset = 0) {
    if (!utility::ArgumentTraits<typename utility::FunctionTraits<F>::Arguments>::IsMatch(info)) {
        throw std::runtime_error("No suitable function found");
    }
    return offset;
}

template<typename F1, typename F2, typename ...FS>
int SelectFunction(const v8::FunctionCallbackInfo<v8::Value> &info, int offset = 0) {
    try {
        return SelectFunction<F1>(info, offset);
    } catch (const std::exception &) {}
    return SelectFunction<F2, FS...>(info, offset + 1);
}

template<typename F>
v8::Local<v8::FunctionTemplate> WrapFunctionImpl(v8::Isolate *isolate, F&& f) {
    v8::EscapableHandleScope scope(isolate);
    using Traits = typename utility::FunctionTraits<F>;
    return scope.Escape(v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
        auto function_pointer = ExternalData::Unwrap<typename Traits::PointerType>(info.Data());
        if constexpr (std::is_member_function_pointer_v<std::remove_reference_t<F>>) {
            auto object = FromV8<typename Traits::ClassType *>(info.GetIsolate(), info.This());
            info.GetReturnValue().Set(CallFunction(function_pointer, info, object));
        } else {
            info.GetReturnValue().Set(CallFunction(function_pointer, info));
        }
    }, ExternalData::New(isolate, std::forward<F>(f))));
}

template<typename ...FS>
v8::Local<v8::FunctionTemplate> WrapFunction(v8::Isolate *isolate, FS&&... fs) {
    v8::EscapableHandleScope scope(isolate);
    std::vector<v8::Local<v8::FunctionTemplate>>
        functions_l { WrapFunctionImpl(isolate, std::forward<FS>(fs))... };
    std::vector<v8::Global<v8::FunctionTemplate>> functions;
    functions.reserve(functions_l.size());
    for (auto &f : functions_l) {
        functions.emplace_back(isolate, f);
    }
    return scope.Escape(v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &info) {
        try {
            v8::Isolate *isolate = info.GetIsolate();
            auto context = isolate->GetCurrentContext();
            auto f = ExternalData::Unwrap<std::vector<v8::Global<v8::FunctionTemplate>>>(
                    info.Data())[SelectFunction<FS...>(info)].Get(isolate)
                    ->GetFunction(context).ToLocalChecked();
            std::vector<v8::Local<v8::Value>> args(info.Length());
            for (int i = 0; i < info.Length(); ++i) {
                args[i] = info[i];
            }
            info.GetReturnValue().Set(f->Call(context, info.This(), info.Length(), args.data()).ToLocalChecked());
        } catch (const std::exception &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(v8_str(e.what())));
        }
    }, ExternalData::New(isolate, std::move(functions))));
}


template<typename T, typename AS, size_t ...Indices>
T *CallConstructorImpl(const v8::FunctionCallbackInfo<v8::Value> &info,
        std::index_sequence<Indices...>) {
    return new T(FromV8<std::tuple_element_t<Indices, AS>>(
                    info.GetIsolate(), info[Indices])...);
}

template<typename T, typename AS>
T *CallConstructor(const v8::FunctionCallbackInfo<v8::Value> &args) {
    if (!utility::ArgumentTraits<AS>::IsMatch(args)) {
        throw std::runtime_error("No suitable constructor found");
    }
    using indices = std::make_index_sequence<std::tuple_size_v<AS>>;
    return CallConstructorImpl<T, AS>(args, indices {});
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
