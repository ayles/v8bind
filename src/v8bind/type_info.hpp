//
// Created by selya on 02.09.2019.
//

#ifndef SANDWICH_V8B_TYPE_INFO_HPP
#define SANDWICH_V8B_TYPE_INFO_HPP

#include <functional>
#include <string>
#include <type_traits>
#include <cstring>

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
    #define FUNCTION_FRONT_DISCARD_SIZE 43u
    #define FUNCTION_BACK_DISCARD_SIZE 7u
    #define UNIQUE_FUNCTION_ID __FUNCSIG__
#else
    #if defined( __GNUC__ ) || defined(__clang__)
        #define FUNCTION_FRONT_DISCARD_SIZE 54u
        #define FUNCTION_BACK_DISCARD_SIZE 1u
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

    TypeInfo(const TypeInfo &other)
            : type_id(other.type_id), size(other.size), name(other.name) {}

    TypeInfo &operator=(const TypeInfo &other) {
        type_id = other.type_id;
        size = other.size;
        name = other.name;
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

    [[nodiscard]]
    const char *GetName() const {
        return name;
    }

#if defined(RTTI_ENABLED)
    template<typename T>
    static TypeInfo Get() {
        return TypeInfo(std::type_index(typeid(T)), sizeof(T), GetName<T>());
    }

    template<typename T>
    static TypeInfo Get(T &&t) {
        return TypeInfo(std::type_index(typeid(std::forward<T>(t))), sizeof(T), GetName<T>());
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
    const char *name;

    explicit TypeInfo(TypeId type_id, size_t size, const char *name)
        : type_id(type_id), size(size), name(name) {}

    template<typename T>
    static const char *GetName() {
        static const size_t size = sizeof(UNIQUE_FUNCTION_ID) - FUNCTION_FRONT_DISCARD_SIZE - FUNCTION_BACK_DISCARD_SIZE;
        static char name[size] = {};
        if (!name[0]) {
            std::memcpy(name, UNIQUE_FUNCTION_ID + FUNCTION_FRONT_DISCARD_SIZE, size - 1u);
        }
        return name;
    }

#if !defined(RTTI_ENABLED)
    static size_t constexpr ConstStringHash(const char *input) {
        return *input
            ? (size_t)((size_t)(*input) + (size_t)(33 * ConstStringHash(input + 1)))
            : 5381;
    }

    template<typename T>
    static TypeInfo GetImpl() {
        constexpr size_t hash = ConstStringHash(UNIQUE_FUNCTION_ID);
        return TypeInfo(hash, sizeof(T), GetName<T>());
    }
#endif
};

}

#undef RTTI_ENABLED
#undef UNIQUE_FUNCTION_ID

#endif //SANDWICH_V8B_TYPE_INFO_HPP
