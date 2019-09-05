//
// Created by selya on 01.09.2019.
//

#ifndef SANDWICH_V8B_CLASS_HPP
#define SANDWICH_V8B_CLASS_HPP

#include <v8bind/type_info.hpp>

#include <v8.h>

#include <unordered_map>
#include <type_traits>

namespace v8b {

class PointerManager {
public:
    friend class ClassManager;

protected:
    virtual void BeginObjectManage(void *ptr) = 0;
    virtual void EndObjectManage(void *ptr) = 0;
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

    void *UnwrapObject(v8::Local<v8::Value> value);

    v8::Local<v8::FunctionTemplate> GetFunctionTemplate() const;

    void SetConstructor(ConstructorFunction constructor_function);
    void SetDestructor(DestructorFunction destructor_function);

    void SetAutoWrap(bool auto_wrap = true);
    bool IsAutoWrapEnabled() const;

    v8::Isolate *GetIsolate() const;

    //
    void BeginObjectManage(void *ptr) override;
    void EndObjectManage(void *ptr) override;

private:
    struct WrappedObject {
        void *ptr;
        v8::Global<v8::Object> wrapped_object;
        PointerManager *pointer_manager;
    };

    void ResetObject(WrappedObject &object);

    std::unordered_map<void *, WrappedObject> objects;

    v8::Isolate *isolate;
    v8::Persistent<v8::FunctionTemplate> function_template;

    ConstructorFunction constructor_function;
    DestructorFunction destructor_function;
    bool auto_wrap;
};

class ClassManagerPool {
public:
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

    template<typename ...Args>
    static void *ObjectCreate(const v8::FunctionCallbackInfo<v8::Value> &args);

    static void ObjectDestroy(v8::Isolate *isolate, void *ptr);

public:
    explicit Class(v8::Isolate *isolate);

    template<typename ...Args>
    Class &Constructor();

    template<typename Member>
    Class &Var(const std::string &name, Member ptr);

    Class &AutoWrap(bool auto_wrap = true);

    v8::Local<v8::FunctionTemplate> GetFunctionTemplate();

    static T *UnwrapObject(v8::Isolate *isolate, v8::Local<v8::Value> value);
    static v8::Local<v8::Object> WrapObject(v8::Isolate *isolate, T *ptr, bool take_ownership);
    static v8::Local<v8::Object> FindObject(v8::Isolate *isolate, T *ptr);
    static v8::Local<v8::Object> FindObject(v8::Isolate *isolate, const T &obj);
};

}

#endif //SANDWICH_V8B_CLASS_HPP

#include <v8bind/class.ipp>
