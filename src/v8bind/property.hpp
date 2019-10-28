//
// Created by selya on 11.09.2019.
//

#ifndef SANDWICH_V8B_PROPERTY_HPP
#define SANDWICH_V8B_PROPERTY_HPP

#include <v8bind/class.hpp>
#include <v8bind/traits.hpp>

#include <cstddef>

namespace v8b::impl {

struct AccessorData {
    v8::AccessorGetterCallback getter;
    v8::AccessorSetterCallback setter;
    v8::Local<v8::Value> data;
    v8::PropertyAttribute attribute;
};

template<bool is_member, typename V>
V8B_IMPL AccessorData VarAccessor(v8::Isolate *isolate, V &&var) {
    auto getter = [](v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info) {
        try {
            auto v = ExternalData::Unwrap<V>(info.Data());
            if constexpr (is_member) {
                static_assert(std::is_member_object_pointer_v<V>, "Var must be pointer to member data");
                auto obj = Class<typename v8b::traits::function_traits<V>::class_type>
                        ::UnwrapObject(info.GetIsolate(), info.This());
                info.GetReturnValue().Set(ToV8(info.GetIsolate(), (*obj).*v));
            } else {
                static_assert(std::is_pointer_v<V>, "V should be a pointer to variable");
                info.GetReturnValue().Set(ToV8(info.GetIsolate(), *v));
            }
        } catch (const V8BindException &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), e.what())));
        }
    };

    v8::AccessorSetterCallback setter = nullptr;
    auto attribute = v8::PropertyAttribute(v8::DontDelete | v8::ReadOnly);
    if constexpr (
            (is_member && !std::is_const_v<typename traits::function_traits<V>::return_type>) ||
            (!is_member && !std::is_const_v<std::remove_pointer_t<V>>)) {

        setter = [](v8::Local<v8::String> property, v8::Local<v8::Value> value,
                    const v8::PropertyCallbackInfo<void> &info) {
            try {
                auto v = ExternalData::Unwrap<V>(info.Data());
                if constexpr (is_member) {
                    auto obj = Class<typename v8b::traits::function_traits<V>::class_type>
                            ::UnwrapObject(info.GetIsolate(), info.This());
                    (*obj).*v = FromV8<typename traits::function_traits<V>::return_type>(info.GetIsolate(), value);
                } else {
                    *v = FromV8<std::remove_pointer_t<V>>(info.GetIsolate(), value);
                }
            } catch (const V8BindException &e) {
                info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), e.what())));
            }
        };
        attribute = v8::DontDelete;
    }

    return AccessorData {
            getter,
            setter,
            ExternalData::New(isolate, std::forward<V>(var)),
            attribute
    };
}

template<bool is_member, typename Getter, typename Setter = std::nullptr_t>
V8B_IMPL AccessorData PropertyAccessor(v8::Isolate *isolate, Getter &&get, Setter &&set) {
    using GetterTrait = typename traits::function_traits<Getter>;
    using SetterTrait = typename traits::function_traits<Setter>;

    std::tuple accessors(std::forward<Getter>(get), std::forward<Setter>(set));

    auto getter = [](v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info) {
        try {
            decltype(auto) acc = ExternalData::Unwrap<decltype(accessors)>(info.Data());
            if constexpr (is_member) {
                static_assert(std::tuple_size_v<typename GetterTrait::arguments> == 1,
                              "Getter function must have no arguments");
                using ClassType = typename std::decay<typename std::tuple_element<0, typename GetterTrait::arguments>::type>::type;
                auto obj = Class<ClassType>::UnwrapObject(info.GetIsolate(), info.This());
                info.GetReturnValue().Set(ToV8(info.GetIsolate(), std::invoke(std::get<0>(acc), *obj)));
            } else {
                static_assert(std::tuple_size_v<typename GetterTrait::arguments> == 0,
                              "Getter function must have no arguments");
                info.GetReturnValue().Set(ToV8(info.GetIsolate(), std::invoke(std::get<0>(acc))));
            }
        } catch (const V8BindException &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), e.what())));
        }
    };

    v8::AccessorSetterCallback setter = nullptr;
    auto attribute = v8::PropertyAttribute(v8::DontDelete | v8::ReadOnly);
    if constexpr (!std::is_same_v<Setter, std::nullptr_t>) {
        setter = [](v8::Local<v8::String> property, v8::Local<v8::Value> value,
                    const v8::PropertyCallbackInfo<void> &info) {
            try {
                decltype(auto) acc = ExternalData::Unwrap<decltype(accessors)>(info.Data());
                if constexpr (is_member) {
                    static_assert(std::tuple_size_v<typename SetterTrait::arguments> == 2,
                                  "Setter function must have 1 argument");
                    using ClassType = std::decay_t<std::tuple_element_t<0, typename SetterTrait::arguments>>;
                    auto obj = Class<ClassType>::UnwrapObject(info.GetIsolate(), info.This());
                    std::invoke(std::get<1>(acc), *obj,
                                FromV8<std::tuple_element_t<1, typename SetterTrait::arguments>>(info.GetIsolate(),
                                                                                                 value));
                } else {
                    static_assert(std::tuple_size_v<typename SetterTrait::arguments> == 1,
                                  "Setter function must have 1 argument");
                    std::invoke(std::get<1>(acc),
                                FromV8<std::tuple_element_t<0, typename SetterTrait::arguments>>(info.GetIsolate(),
                                                                                                 value));
                }
            } catch (const V8BindException &e) {
                info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), e.what())));
            }
        };
        attribute = v8::DontDelete;
    }

    return AccessorData {
            getter,
            setter,
            ExternalData::New(isolate, std::move(accessors)),
            attribute
    };
}

}

#endif //SANDWICH_V8B_PROPERTY_HPP
