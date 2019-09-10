//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_CONVERT_HPP
#define SANDWICH_V8B_CONVERT_HPP

#include <v8bind/class.hpp>

#include <v8.h>

#include <type_traits>
#include <string>
#include <stdexcept>
#include <exception>
#include <memory>

namespace v8b {

template<typename T, typename Enable = void>
struct Convert;

template<typename T>
struct Convert<v8::Local<T>> {
    using CType = v8::Local<T>;
    using V8Type = v8::Local<T>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.As<T>().IsEmpty();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not of type");
        }
        return value.As<T>();
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return value;
    }
};

template<>
struct Convert<bool> {
    using CType = bool;
    using V8Type = v8::Local<v8::Boolean>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsBoolean();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid bool");
        }
        return value.As<v8::Boolean>()->Value();
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return v8::Boolean::New(isolate, value);
    }
};

template<typename T>
struct Convert<T, std::enable_if_t<std::is_floating_point_v<T> || std::is_integral_v<T>>> {
    using CType = T;
    using V8Type = v8::Local<v8::Number>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsNumber();
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid number");
        }
        return static_cast<T>(value.As<v8::Number>()->Value());
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return v8::Number::New(isolate, static_cast<double>(value));
    }
};

template<typename Char, typename Traits, typename Alloc>
struct Convert<std::basic_string<Char, Traits, Alloc>> {
    static_assert(sizeof(Char) <= sizeof(uint16_t),
                  "Only UTF-8 and UTF-16 strings are supported");

    using CType = std::basic_string<Char, Traits, Alloc>;
    using V8Type = v8::Local<v8::String>;

    static bool IsValid(v8::Isolate *, v8::Local<v8::Value> value) {
        return !value.IsEmpty() && value->IsString();
    }

    static CType FromV8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid string");
        }
        if constexpr (sizeof(Char) == 1) {
            const v8::String::Utf8Value str(isolate, value);
            return CType(reinterpret_cast<const Char *>(*str), str.length());
        } else {
            const v8::String::Value str(isolate, value);
            return CType(reinterpret_cast<const Char *>(*str), str.length());
        }
    }

    static V8Type ToV8(v8::Isolate* isolate, const CType &value) {
        if constexpr (sizeof(Char) == 1) {
            return v8::String::NewFromUtf8(
                    isolate,
                    reinterpret_cast<const char *>(value.data()),
                    v8::NewStringType::kNormal,
                    static_cast<int>(value.size())
            ).ToLocalChecked();
        } else {
            return v8::String::NewFromTwoByte(
                    isolate,
                    reinterpret_cast<uint16_t const*>(value.data()),
                    v8::NewStringType::kNormal,
                    static_cast<int>(value.size())
            ).ToLocalChecked();
        }
    }
};

template<typename T>
struct IsWrappedClass : std::is_class<T> {};

template<typename T>
struct IsWrappedClass<v8::Local<T>> : std::false_type {};

template<typename T>
struct IsWrappedClass<v8::Global<T>> : std::false_type {};

template<typename Char, typename Traits, typename Alloc>
struct IsWrappedClass<std::basic_string<Char, Traits, Alloc>> : std::false_type {};

template<typename T>
struct IsWrappedClass<std::shared_ptr<T>> : std::false_type {};


template<typename T>
struct Convert<T *, typename std::enable_if_t<IsWrappedClass<T>::value>> {
    using CType = T *;
    using V8Type = v8::Local<v8::Object>;

    static bool IsValid(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (value.IsEmpty() || !value->IsObject()) {
            return false;
        }
        try {
            Class<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
        } catch (const std::exception &e) {
            return false;
        }
        return true;
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid object");
        }
        return Class<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return Class<std::remove_cv_t<T>>::FindObject(isolate, value);
    }
};

template<typename T>
struct Convert<T, typename std::enable_if_t<IsWrappedClass<T>::value>> {
    using CType = T &;
    using V8Type = v8::Local<v8::Object>;

    static bool IsValid(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        return Convert<T *>::IsValid(isolate, value);
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid object");
        }
        auto ptr = Class<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
        if (!ptr) {
            throw std::runtime_error("Failed to unwrap object");
        }
        return *ptr;
    }

    static V8Type ToV8(v8::Isolate *isolate, const T &value) {
        auto wrapped = Class<std::remove_cv_t<T>>::FindObject(isolate, value);
        if (wrapped.IsEmpty()) {
            throw std::runtime_error("Failed to wrap object");
        }
        return wrapped;
    }
};

template<typename T>
struct Convert<std::shared_ptr<T>, typename std::enable_if_t<IsWrappedClass<T>::value>> {
    using CType = std::shared_ptr<T>;
    using V8Type = v8::Local<v8::Object>;

    static bool IsValid(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        return Convert<T>::IsValid(isolate, value);
    }

    static CType FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        if (!IsValid(isolate, value)) {
            throw std::runtime_error("Value is not a valid object");
        }
        return SharedPointerManager<std::remove_cv_t<T>>::UnwrapObject(isolate, value);
    }

    static V8Type ToV8(v8::Isolate *isolate, CType value) {
        return SharedPointerManager<std::remove_cv_t<T>>::FindObject(isolate, value);
    }
};



template<typename T>
struct Convert<T &> : Convert<T> {};

template<typename T>
struct Convert<const T &> : Convert<T> {};

template<typename T>
decltype(auto) FromV8(v8::Isolate *isolate, v8::Local<v8::Value> value) {
    return Convert<T>::FromV8(isolate, value);
}

template<typename T>
decltype(auto) ToV8(v8::Isolate *isolate, T &&t) {
    return Convert<T>::ToV8(isolate, std::forward<T>(t));
}

inline decltype(auto) ToV8(v8::Isolate *isolate, const char *c) {
    return Convert<std::string>::ToV8(isolate, std::string(c));
}

}

#endif //SANDWICH_V8B_CONVERT_HPP
