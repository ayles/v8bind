//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_CLASS_HPP
#define SANDWICH_V8B_CLASS_HPP

#include <v8bind/type_info.hpp>

#include <v8.h>

#include <unordered_map>
#include <type_traits>
#include <memory>
#include <vector>
#include <cstddef>
#include <tuple>

#define V8B_IMPL inline

namespace v8b {

class PointerManager {
public:
    friend class ClassManager;

protected:
    virtual void EndObjectManage(void *ptr) = 0;

public:
    template<typename T>
    static std::enable_if_t<std::is_base_of_v<PointerManager, T>, T &> GetInstance() {
        static T instance;
        return instance;
    }
};

class ClassManager : public PointerManager {
public:
    const TypeInfo type_info;

    using ConstructorFunction = void * (*)(const v8::FunctionCallbackInfo<v8::Value> &);
    using DestructorFunction = void (*)(v8::Isolate *, void *);

    ClassManager(v8::Isolate *isolate, const TypeInfo &type_info);
    ~ClassManager();

    // Delete unwanted constructors and operators
    ClassManager(const ClassManager &) = delete;
    ClassManager &operator=(const ClassManager &) = delete;
    ClassManager &operator=(ClassManager &&) = delete;

    // Leave constructor with move semantics
    ClassManager(ClassManager &&) = default;

    void RemoveObject(void *ptr);
    void RemoveObjects();

    v8::Local<v8::Object> FindObject(void *ptr) const;
    v8::Local<v8::Object> WrapObject(void *ptr);
    v8::Local<v8::Object> WrapObject(void *ptr, PointerManager *pointer_manager);
    void SetPointerManager(void *ptr, PointerManager *pointer_manager);

    void *UnwrapObject(v8::Local<v8::Value> value);

    [[nodiscard]]
    v8::Local<v8::FunctionTemplate> GetFunctionTemplate() const;

    void SetBase(const TypeInfo &type_info,
            void *(*base_to_this)(void *), void *(*this_to_base)(void *));

    void SetConstructor(ConstructorFunction constructor_function);
    void SetDestructor(DestructorFunction destructor_function);

    void SetAutoWrap(bool auto_wrap = true);
    // Pointer auto wrap is almost newer required so use it with caution
    void SetPointerAutoWrap(bool auto_wrap = true);

    [[nodiscard]]
    bool IsAutoWrapEnabled() const;

    [[nodiscard]]
    bool IsPointerAutoWrapEnabled() const;

    [[nodiscard]]
    v8::Isolate *GetIsolate() const;

    void EndObjectManage(void *ptr) override;

private:
    struct WrappedObject {
        void *ptr;
        v8::Global<v8::Object> wrapped_object;
        PointerManager *pointer_manager;
    };

    WrappedObject &FindWrappedObject(void *ptr, void **base_ptr_ptr = nullptr) const;
    void ResetObject(WrappedObject &object);

    std::unordered_map<void *, WrappedObject> objects;

    v8::Isolate *isolate;
    v8::Persistent<v8::FunctionTemplate> function_template;

    ConstructorFunction constructor_function;
    DestructorFunction destructor_function;

    struct BaseClassInfo {
        ClassManager *base_class_manager = nullptr;
        void *(*base_to_this)(void *) = nullptr;
        void *(*this_to_base)(void *) = nullptr;
    };

    BaseClassInfo base_class_info;
    std::vector<ClassManager *> derived_class_managers;

    bool auto_wrap;
    bool pointer_auto_wrap;
};

class ClassManagerPool {
public:
    template<typename T>
    static ClassManager &Get(v8::Isolate *isolate);

    static ClassManager &Get(v8::Isolate *isolate, const TypeInfo &type_info);
    static void Remove(v8::Isolate *isolate, const TypeInfo &type_info);
    static void RemoveAll(v8::Isolate *isolate);

private:
    std::vector<std::unique_ptr<ClassManager>> managers;

    static std::unordered_map<v8::Isolate *, ClassManagerPool> pools;

    static ClassManagerPool &GetInstance(v8::Isolate *isolate);
    static void RemoveInstance(v8::Isolate *isolate);
};

template<typename T>
class Class {
    ClassManager &class_manager;

public:
    explicit Class(v8::Isolate *isolate);

    template<typename B>
    Class &Inherit();

    template<typename ...Args>
    Class &Constructor();

    template<typename ...F>
    Class &Constructor(F&&... f);

    template<typename U>
    Class &Subclass(const std::string &name, const v8b::Class<U> &cl);

    template<typename Data>
    Class &Value(const std::string &name, v8::Local<Data> value, v8::PropertyAttribute attribute = v8::None);

    template<typename Member>
    Class &Var(const std::string &name, Member &&ptr);

    template<typename Getter, typename Setter = std::nullptr_t>
    Class &Property(const std::string &name, Getter &&getter, Setter &&setter = nullptr);

    template<typename Getter, typename Setter = std::nullptr_t>
    Class &Indexer(Getter &&getter, Setter &&setter = nullptr);

    template<typename ...F>
    Class &Function(const std::string &name, F&&... f);

    template<typename Data>
    Class &StaticValue(const std::string &name, v8::Local<Data> value, v8::PropertyAttribute attribute = v8::None);

    template<typename V>
    Class &StaticVar(const std::string &name, V &&v);

    template<typename Getter, typename Setter = std::nullptr_t>
    Class &StaticProperty(const std::string &name, Getter &&getter, Setter &&setter = nullptr);

    template<typename ...F>
    Class &StaticFunction(const std::string &name, F&&... f);

    Class &AutoWrap(bool auto_wrap = true);
    Class &PointerAutoWrap(bool auto_wrap = true);

    [[nodiscard]]
    v8::Local<v8::FunctionTemplate> GetFunctionTemplate() const;

    static T *UnwrapObject(v8::Isolate *isolate, v8::Local<v8::Value> value);
    static v8::Local<v8::Object> WrapObject(v8::Isolate *isolate, T *ptr, bool take_ownership);
    static v8::Local<v8::Object> WrapObject(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager);
    static v8::Local<v8::Object> FindObject(v8::Isolate *isolate, T *ptr);
    static v8::Local<v8::Object> FindObject(v8::Isolate *isolate, const T &obj);
    static void SetPointerManager(v8::Isolate *isolate, T *ptr, PointerManager *pointerManager);

private:
    static bool initialized;
};

template<typename T>
class SharedPointerManager : public PointerManager {
    std::unordered_map<T *, std::shared_ptr<T>> pointers;

public:
    V8B_IMPL static v8::Local<v8::Object> WrapObject(v8::Isolate *isolate, const std::shared_ptr<T> &ptr) {
        auto &instance = PointerManager::GetInstance<SharedPointerManager>();
        auto res = Class<T>::WrapObject(isolate, ptr.get(), &instance);
        instance.pointers.emplace(ptr.get(), ptr);
        return res;
    }

    V8B_IMPL static v8::Local<v8::Object> FindObject(v8::Isolate *isolate, const std::shared_ptr<T> &ptr) {
        auto &instance = PointerManager::GetInstance<SharedPointerManager>();
        auto object = Class<T>::FindObject(isolate, ptr.get());
        if (instance.pointers.find(ptr.get()) == instance.pointers.end()) {
            Class<T>::SetPointerManager(isolate, ptr.get(), &instance);
            instance.pointers.emplace(ptr.get(), ptr);
        }
        return object;
    }

    V8B_IMPL static std::shared_ptr<T> UnwrapObject(v8::Isolate *isolate, v8::Local<v8::Value> value) {
        auto &instance = PointerManager::GetInstance<SharedPointerManager>();
        auto ptr = Class<T>::UnwrapObject(isolate, value);
        if (instance.pointers.find(ptr) == instance.pointers.end()) {
            Class<T>::SetPointerManager(isolate, ptr, &instance);
            instance.pointers.emplace(ptr, std::shared_ptr<T>(ptr));
        }
        return instance.pointers.find(ptr)->second;
    }

protected:
    V8B_IMPL void EndObjectManage(void *ptr) override {
        pointers.erase(static_cast<T *>(ptr));
    }
};

}

#endif //SANDWICH_V8B_CLASS_HPP
