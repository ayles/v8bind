//
// Created by selya on 02.09.2019.
//

#ifndef SANDWICH_V8B_TYPE_INFO_HPP
#define SANDWICH_V8B_TYPE_INFO_HPP

#include <functional>
#include <string>
#include <type_traits>

// Check RTTI
#if defined(__clang__)
    #if __has_feature(cxx_rtti)
        #define RTTI_ENABLED
    #endif
#elif defined(__GNUG__)
    #if defined(__GXX_RTTI)
        #define RTTI_ENABLED
    #endif
#elif defined(_MSC_VER)
    #if defined(_CPPRTTI)
        #define RTTI_ENABLED
    #endif
#endif

// Pretty function
#if defined(_MSC_VER)
    #define UNIQUE_FUNCTION_ID __FUNCSIG__
#else
    #if defined( __GNUC__ ) || defined(__clang__)
        #define UNIQUE_FUNCTION_ID __PRETTY_FUNCTION__
    #endif
#endif

#if defined(RTTI_ENABLED)
#include <typeindex>
#endif

namespace v8b {

struct TypeInfo {
#if defined(RTTI_ENABLED)
    using TypeId = std::type_index;
#else
    using TypeId = size_t;
#endif

    TypeInfo(const TypeInfo &other, size_t size) : type_id(other.type_id), size(size) {}
    TypeInfo &operator=(const TypeInfo &other) {
        type_id = other.type_id;
        size = other.size;
        return *this;
    }

    bool operator<(const TypeInfo& other) const {
        return type_id < other.type_id;
    }

    bool operator>(const TypeInfo& other) const {
        return type_id > other.type_id;
    }

    bool operator>=(const TypeInfo& other) const {
        return type_id >= other.type_id;
    }

    bool operator<=(const TypeInfo& other) const {
        return type_id <= other.type_id;
    }

    bool operator==(const TypeInfo& other) const {
        return type_id == other.type_id;
    }

    bool operator!=(const TypeInfo& other) const {
        return type_id != other.type_id;
    }

    [[nodiscard]]
    TypeId GetTypeId() const {
        return type_id;
    }

    [[nodiscard]]
    size_t GetSize() const {
        return size;
    }

#if defined(RTTI_ENABLED)
    template<typename T>
    static TypeInfo Get() {
        return TypeInfo(std::type_index(typeid(T)));
    }

    template<typename T>
    static TypeInfo Get(T &&t) {
        return TypeInfo(std::type_index(typeid(std::forward<T>(t))));
    }
#else
    template<typename T>
    static TypeInfo Get() {
        return GetImpl<std::decay_t<T>>();
    }

    template<typename T>
    static TypeInfo Get(const T *) {
        return Get<T>();
    }

    template<typename T>
    static TypeInfo Get(T &&) {
        return Get<T>();
    }
#endif

private:
    TypeId type_id;
    size_t size;

    explicit TypeInfo(TypeId type_id) : type_id(type_id) {}

#if !defined(RTTI_ENABLED)
    static size_t constexpr ConstStringHash(const char *input) {
        return *input
            ? (size_t)((size_t)(*input) + (size_t)(33 * ConstStringHash(input + 1)))
            : 5381;
    }

    template<typename T>
    static TypeInfo GetImpl() {
        constexpr size_t hash = ConstStringHash(UNIQUE_FUNCTION_ID);
        return TypeInfo(hash);
    }
#endif
};

}

#undef RTTI_ENABLED
#undef UNIQUE_FUNCTION_ID

#endif //SANDWICH_V8B_TYPE_INFO_HPP
