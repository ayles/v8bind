//
// Created by selya on 03.09.2019.
//

#ifndef SANDWICH_V8B_CLASS_IPP
#define SANDWICH_V8B_CLASS_IPP

#include <v8bind/class.hpp>
#include <v8bind/function.hpp>
#include <v8bind/default_bindings.hpp>
#include <v8bind/property.hpp>

#include <v8.h>

#include <exception>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <utility>
#include <functional>
#include <tuple>

namespace v8b {

V8B_IMPL ClassManager::ClassManager(v8::Isolate *isolate, const v8b::TypeInfo &type_info)
        : type_info(type_info), isolate(isolate), auto_wrap(false),
        pointer_auto_wrap(false), constructor_function(nullptr), destructor_function(nullptr) {
    v8::HandleScope scope(isolate);

    auto f = v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &args) {
        auto self = ExternalData::Unwrap<ClassManager *>(args.Data());
        try {
            if (!self->constructor_function) {
                throw std::runtime_error("No constructor specified");
            }
            args.GetReturnValue().Set(self->WrapObject(self->constructor_function(args)));
        } catch (const std::exception &e) {
            args.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(args.GetIsolate(), std::string(e.what()))));
        }
    }, ExternalData::New(isolate, this));

    function_template.Reset(isolate, f);

    // 0 - raw pointer to C++ object
    // 1 - pointer to this ClassManager
    f->InstanceTemplate()->SetInternalFieldCount(2);
}

V8B_IMPL ClassManager::~ClassManager() {
    RemoveObjects();
}

V8B_IMPL void ClassManager::RemoveObject(void *ptr) {
    auto it = objects.find(ptr);
    if (it == objects.end()) {
        throw std::runtime_error("Can't remove unmanaged object");
    }
    v8::HandleScope scope(isolate);
    ResetObject(it->second);
    objects.erase(it);
}

V8B_IMPL void ClassManager::RemoveObjects() {
    v8::HandleScope scope(isolate);
    for (auto &p : objects) {
        ResetObject(p.second);
    }
    objects.clear();
}

V8B_IMPL ClassManager::WrappedObject &ClassManager::FindWrappedObject(void *ptr, void **base_ptr_ptr) const {
    // TODO
    auto it = objects.find(ptr);
    if (it == objects.end()) {
        for (ClassManager *derived : derived_class_managers) {
            try {
                if (base_ptr_ptr) {
                    auto &o = derived->FindWrappedObject(ptr, base_ptr_ptr);
                    *base_ptr_ptr = derived->base_class_info.this_to_base(*base_ptr_ptr);
                    return o;
                } else {
                    // Downcast only if ptr is base ptr, else return upcasted base_ptr in
                    // base_ptr_ptr
                    return derived->FindWrappedObject(derived->base_class_info.base_to_this(ptr));
                }
            } catch (const std::exception &) {
                continue;
            }
        }
        throw std::runtime_error("Can't find wrapped object");
    }
    if (base_ptr_ptr) {
        *base_ptr_ptr = ptr;
    }
    return const_cast<WrappedObject &>(it->second);
}

V8B_IMPL void ClassManager::ResetObject(WrappedObject &object) {
    if (object.pointer_manager) {
        object.pointer_manager->EndObjectManage(object.ptr);
    }
    object.wrapped_object.Reset();
    isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(type_info.GetSize()));
}

V8B_IMPL v8::Local<v8::Object> ClassManager::FindObject(void *ptr) const {
    return FindWrappedObject(ptr).wrapped_object.Get(isolate);
}

V8B_IMPL void ClassManager::SetPointerManager(void *ptr, PointerManager *pointer_manager) {
    if (pointer_manager == nullptr) {
        throw std::runtime_error("Can't set nullptr as pointer manager");
    }

    auto it = objects.find(ptr);
    if (it == objects.end()) {
        try {
            FindWrappedObject(ptr);
        } catch (const std::exception &) {
            throw std::runtime_error("Can't find object");
        }
        throw std::runtime_error("Setting pointer manager for object through his base is not allowed");
    }

    if (it->second.pointer_manager != nullptr && it->second.pointer_manager != this) {
        throw std::runtime_error("Custom pointer manager already set");
    }

    it->second.pointer_manager = pointer_manager;
}

V8B_IMPL v8::Local<v8::Object> ClassManager::WrapObject(void *ptr) {
    return WrapObject(ptr, this);
}

V8B_IMPL v8::Local<v8::Object> ClassManager::WrapObject(void *ptr, PointerManager *pointer_manager) {
    if (!ptr) {
        return v8::Local<v8::Object>();
    }

    v8::EscapableHandleScope scope(isolate);

    bool found = false;
    try {
        FindWrappedObject(ptr);
        found = true;
    } catch (const std::exception &) {
        // Object not find, it's all ok
    }
    if (found) {
        throw std::runtime_error("Object is already wrapped");
    }

    auto context = isolate->GetCurrentContext();
    auto wrapped = function_template.Get(isolate)
            ->InstanceTemplate()->NewInstance(context).ToLocalChecked();

    wrapped->SetAlignedPointerInInternalField(0, ptr);
    wrapped->SetAlignedPointerInInternalField(1, this);

    v8::Global<v8::Object> global(isolate, wrapped);
    global.SetWeak(this, [](const v8::WeakCallbackInfo<ClassManager> &data) {
        void *ptr = data.GetInternalField(0);
        auto self = static_cast<ClassManager *>(data.GetInternalField(1));
        self->RemoveObject(ptr);
    }, v8::WeakCallbackType::kInternalFields);

    objects.emplace(ptr, WrappedObject {
        ptr,
        std::move(global),
        pointer_manager
    });

    isolate->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(type_info.GetSize()));

    /*if (pointer_manager != nullptr) {
        pointer_manager->BeginObjectManage(ptr);
    }*/

    return scope.Escape(wrapped);
}

V8B_IMPL void *ClassManager::UnwrapObject(v8::Local<v8::Value> value) {
    v8::HandleScope scope(isolate);

    if (!value->IsObject()) {
        throw std::runtime_error("Can't unwrap - not an object");
    }

    auto obj = value.As<v8::Object>();

    if (obj->InternalFieldCount() != 2) {
        throw std::runtime_error("Object internal field count != 2");
    }

    void *ptr = obj->GetAlignedPointerFromInternalField(0);

    void *base_ptr = nullptr;
    auto &wrapped_object = FindWrappedObject(ptr, &base_ptr);

    return base_ptr;
}

V8B_IMPL v8::Local<v8::FunctionTemplate> ClassManager::GetFunctionTemplate() const {
    return function_template.Get(isolate);
}

V8B_IMPL void ClassManager::SetBase(const v8b::TypeInfo &type_info,
        void *(*base_to_this)(void *), void *(*this_to_base)(void *)) {
    base_class_info = BaseClassInfo {
        &ClassManagerPool::Get(isolate, type_info),
        base_to_this,
        this_to_base
    };
    base_class_info.base_class_manager->derived_class_managers.emplace_back(this);
    function_template.Get(isolate)->Inherit(
            base_class_info.base_class_manager->function_template.Get(
            base_class_info.base_class_manager->isolate));
}

V8B_IMPL void ClassManager::SetConstructor(ConstructorFunction constructor_function) {
    this->constructor_function = constructor_function;
}

V8B_IMPL void ClassManager::SetDestructor(DestructorFunction destructor_function) {
    this->destructor_function = destructor_function;
}

V8B_IMPL void ClassManager::SetAutoWrap(bool auto_wrap) {
    this->auto_wrap = auto_wrap;
}

V8B_IMPL void ClassManager::SetPointerAutoWrap(bool auto_wrap) {
    this->pointer_auto_wrap = auto_wrap;
}

V8B_IMPL bool ClassManager::IsAutoWrapEnabled() const {
    return auto_wrap;
}

V8B_IMPL bool ClassManager::IsPointerAutoWrapEnabled() const {
    return pointer_auto_wrap;
}

V8B_IMPL v8::Isolate *ClassManager::GetIsolate() const {
    return isolate;
}

V8B_IMPL void ClassManager::EndObjectManage(void *ptr) {
    destructor_function(isolate, ptr);
}

template<typename T>
V8B_IMPL ClassManager &ClassManagerPool::Get(v8::Isolate *isolate) {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        DefaultBindings<T>::Initialize(isolate);
    }
    return Get(isolate, TypeInfo::Get<T>());
}

V8B_IMPL ClassManager &ClassManagerPool::Get(v8::Isolate *isolate, const TypeInfo &type_info) {
    auto &pool = GetInstance(isolate);
    auto it = std::find_if(pool.managers.begin(), pool.managers.end(),
            [&type_info](auto &class_manager) {
        return class_manager->type_info == type_info;
    });
    if (it == pool.managers.end()) {
        it = pool.managers.emplace(pool.managers.end(), new ClassManager(isolate, type_info));
    }
    return *static_cast<ClassManager *>(it->get());
}

V8B_IMPL void ClassManagerPool::Remove(v8::Isolate *isolate, const v8b::TypeInfo &type_info) {
    auto &pool = GetInstance(isolate);
    auto it = std::find_if(pool.managers.begin(), pool.managers.end(),
            [&type_info](auto &class_manager) {
        return class_manager->type_info == type_info;
    });
    if (it == pool.managers.end()) {
        throw std::runtime_error("Can't find ClassManager instance to delete");
    }
    pool.managers.erase(it);
    if (pool.managers.empty()) {
        RemoveInstance(isolate);
    }
}

V8B_IMPL void ClassManagerPool::RemoveAll(v8::Isolate *isolate) {
    RemoveInstance(isolate);
}

V8B_IMPL std::unordered_map<v8::Isolate *, ClassManagerPool> ClassManagerPool::pools;

V8B_IMPL ClassManagerPool &ClassManagerPool::GetInstance(v8::Isolate *isolate) {
    return pools[isolate];
}

V8B_IMPL void ClassManagerPool::RemoveInstance(v8::Isolate *isolate) {
    pools.erase(isolate);
}



template<typename T>
V8B_IMPL Class<T>::Class(v8::Isolate *isolate)
        : class_manager(ClassManagerPool::Get<T>(isolate)) {
    class_manager.SetDestructor([](v8::Isolate *isolate, void *ptr) {
        auto obj = static_cast<T *>(ptr);
        delete obj;
    });
}

template<typename T>
template<typename B>
V8B_IMPL Class<T> &Class<T>::Inherit() {
    static_assert(std::is_base_of_v<B, T>,
            "Class B should be base for class T");
    class_manager.SetBase(TypeInfo::Get<B>(),
            [](void *base_ptr) -> void * {
                return static_cast<void *>(static_cast<T *>(static_cast<B *>(base_ptr)));
            },
            [](void *this_ptr) -> void * {
                return static_cast<void *>(static_cast<B *>(static_cast<T *>(this_ptr)));
            }
    );
    return *this;
}

template<typename T>
template<typename ...Args>
V8B_IMPL Class<T> &Class<T>::Constructor() {
    class_manager.SetConstructor([](const v8::FunctionCallbackInfo<v8::Value> &args) -> void * {
        return CallConstructor<T, Args...>(args);
    });
    return *this;
}

template<typename T>
template<typename ...F>
V8B_IMPL Class<T> &Class<T>::Constructor(F&&... f) {
    WrapConstructor(class_manager.GetIsolate(), std::forward<F>(f)..., class_manager.GetFunctionTemplate());
    return *this;
}

template<typename T>
template<typename Member>
V8B_IMPL Class<T> &Class<T>::Var(const std::string &name, Member &&ptr) {
    v8::HandleScope scope(class_manager.GetIsolate());

    auto data = impl::VarAccessor<true>(class_manager.GetIsolate(), std::forward<Member>(ptr));

    class_manager.GetFunctionTemplate()->PrototypeTemplate()->SetAccessor(
            ToV8(class_manager.GetIsolate(), name),
            data.getter,
            data.setter,
            data.data,
            v8::AccessControl::DEFAULT,
            data.attribute
    );

    return *this;
}

template<typename T>
template<typename Getter, typename Setter>
V8B_IMPL Class<T> &Class<T>::Property(const std::string &name, Getter &&get, Setter &&set) {
    v8::HandleScope scope(class_manager.GetIsolate());

    auto data = impl::PropertyAccessor<true, Getter, Setter>(class_manager.GetIsolate(),
            std::forward<Getter>(get), std::forward<Setter>(set));

    class_manager.GetFunctionTemplate()->PrototypeTemplate()->SetAccessor(
            ToV8(class_manager.GetIsolate(), name),
            data.getter,
            data.setter,
            data.data,
            v8::AccessControl::DEFAULT,
            data.attribute
    );

    return *this;
}

template<typename T>
template<typename Getter, typename Setter>
V8B_IMPL Class<T> &Class<T>::Indexer(Getter &&get, Setter &&set) {
    using GetterTrait = typename traits::function_traits<Getter>;
    using SetterTrait = typename traits::function_traits<Setter>;

    static_assert(std::tuple_size_v<typename GetterTrait::arguments> == 2 &&
                  std::is_integral_v<std::tuple_element_t<1, typename GetterTrait::arguments>>,
                  "Getter function must have one integral argument");
    if constexpr (!std::is_same_v<Setter, std::nullptr_t>) {
        static_assert((std::tuple_size_v<typename SetterTrait::arguments> == 3 &&
                       std::is_integral_v<std::tuple_element_t<1, typename GetterTrait::arguments>>),
                      "Setter function must have 2 arguments with first integral");
    }

    v8::HandleScope scope(class_manager.GetIsolate());

    std::tuple accessors(std::forward<Getter>(get), std::forward<Setter>(set));

    v8::IndexedPropertyGetterCallback getter = [](uint32_t index,
       const v8::PropertyCallbackInfo<v8::Value> &info) {
        try {
            auto obj = UnwrapObject(info.GetIsolate(), info.This());
            auto acc = ExternalData::Unwrap<decltype(accessors)>(info.Data());
            info.GetReturnValue().Set(ToV8(info.GetIsolate(), std::invoke(std::get<0>(acc), *obj, index)));
        } catch (const std::exception &e) {
            info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), std::string(e.what()))));
        }
    };

    v8::IndexedPropertySetterCallback setter = nullptr;
    if constexpr (!std::is_same_v<Setter, std::nullptr_t>) {
        setter = [](uint32_t index, v8::Local<v8::Value> value,
            const v8::PropertyCallbackInfo<v8::Value> &info) {
            try {
                auto obj = UnwrapObject(info.GetIsolate(), info.This());
                auto acc = ExternalData::Unwrap<decltype(accessors)>(info.Data());
                std::invoke(std::get<1>(acc), *obj, index,
                        FromV8<std::tuple_element_t<1, typename SetterTrait::arguments>>(info.GetIsolate(), value));
            } catch (const std::exception &e) {
                info.GetIsolate()->ThrowException(v8::Exception::Error(ToV8(info.GetIsolate(), std::string(e.what()))));
            }
        };
    }

    class_manager.GetFunctionTemplate()->InstanceTemplate()->SetIndexedPropertyHandler(
            getter,
            setter,
            nullptr,
            nullptr,
            nullptr,
            ExternalData::New(class_manager.GetIsolate(), std::move(accessors))
    );

    return *this;
}

template<typename T>
template<typename ...F>
V8B_IMPL Class<T> &Class<T>::Function(const std::string &name, F&&... f) {
    //static_assert(traits::multi_and(std::is_member_function_pointer_v<F>...),
    //            "All f's must be pointers to member functions");

    v8::HandleScope scope(class_manager.GetIsolate());

    class_manager.GetFunctionTemplate()->PrototypeTemplate()->Set(ToV8(class_manager.GetIsolate(), name),
            WrapFunction<MemberCall>(class_manager.GetIsolate(), std::forward<F>(f)...));

    return *this;
}

template<typename T>
template<typename... F>
V8B_IMPL Class<T> &Class<T>::StaticFunction(const std::string &name, F &&... f) {
    v8::HandleScope scope(class_manager.GetIsolate());

    class_manager.GetFunctionTemplate()->Set(
            ToV8(class_manager.GetIsolate(), name),
            WrapFunction<StaticCall>(class_manager.GetIsolate(), std::forward<F>(f)...));

    return *this;
}

template<typename T>
template<typename V>
V8B_IMPL Class<T> &Class<T>::StaticVar(const std::string &name, V &&v) {
    v8::HandleScope scope(class_manager.GetIsolate());

    auto data = impl::VarAccessor<false>(class_manager.GetIsolate(), std::forward<V>(v));

    class_manager.GetFunctionTemplate()->SetNativeDataProperty(
            ToV8(class_manager.GetIsolate(), name),
            data.getter,
            data.setter,
            data.data,
            data.attribute
    );

    return *this;
}

template<typename T>
template<typename Getter, typename Setter>
V8B_IMPL Class<T> &Class<T>::StaticProperty(const std::string &name, Getter &&get, Setter &&set) {
    v8::HandleScope scope(class_manager.GetIsolate());

    auto data = impl::PropertyAccessor<false, Getter, Setter>(class_manager.GetIsolate(),
                                                               std::forward<Getter>(get), std::forward<Setter>(set));

    class_manager.GetFunctionTemplate()->SetNativeDataProperty(
            ToV8(class_manager.GetIsolate(), name),
            data.getter,
            data.setter,
            data.data,
            data.attribute
    );

    return *this;
}

template<typename T>
V8B_IMPL Class<T> &Class<T>::AutoWrap(bool auto_wrap) {
    class_manager.SetAutoWrap(auto_wrap);
    return *this;
}

template<typename T>
V8B_IMPL Class<T> &Class<T>::PointerAutoWrap(bool auto_wrap) {
    class_manager.SetPointerAutoWrap(auto_wrap);
    return *this;
}

template<typename T>
V8B_IMPL v8::Local<v8::FunctionTemplate> Class<T>::GetFunctionTemplate() const {
    return class_manager.GetFunctionTemplate();
}

template<typename T>
V8B_IMPL T *Class<T>::UnwrapObject(v8::Isolate *isolate, v8::Local<v8::Value> value) {
    return static_cast<T *>(ClassManagerPool::Get<T>(isolate).UnwrapObject(value));
}

template<typename T>
V8B_IMPL v8::Local<v8::Object> Class<T>::WrapObject(v8::Isolate *isolate, T *ptr, bool take_ownership) {
    if (!take_ownership) {
        return ClassManagerPool::Get<T>(isolate).WrapObject(ptr, nullptr);
    }
    return ClassManagerPool::Get<T>(isolate).WrapObject(ptr);
}

template<typename T>
V8B_IMPL v8::Local<v8::Object> Class<T>::WrapObject(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager) {
    return ClassManagerPool::Get<T>(isolate).WrapObject(ptr, pointer_manager);
}

template<typename T>
V8B_IMPL void Class<T>::SetPointerManager(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager) {
    ClassManagerPool::Get<T>(isolate).SetPointerManager(ptr, pointer_manager);
}

template<typename T>
V8B_IMPL v8::Local<v8::Object> Class<T>::FindObject(v8::Isolate *isolate, const T &obj) {
    auto &class_manager = ClassManagerPool::Get<T>(isolate);
    try {
        return class_manager.FindObject(const_cast<T *>(&obj));
    } catch (const std::exception &e) {
        if (!class_manager.IsAutoWrapEnabled()) {
            throw e;
        }
        // Return wrapped clone
        // Useful for class member fields
        // Clone because if it will be just referenced,
        // it will be possible to save this object anywhere in js
        // and access it, but setting member will just set object fields
        // so it will be not bidirectional
        // HINT: store members as pointer to prevent cloning
        return class_manager.WrapObject(new T(obj));
    }
}

template<typename T>
V8B_IMPL v8::Local<v8::Object> Class<T>::FindObject(v8::Isolate *isolate, T *ptr) {
    auto &class_manager = ClassManagerPool::Get<T>(isolate);
    try {
        return class_manager.FindObject(ptr);
    } catch (const std::exception &e) {
        if (!class_manager.IsPointerAutoWrapEnabled()) {
            throw e;
        }
        return class_manager.WrapObject(ptr);
    }
}


}

#endif //SANDWICH_V8B_CLASS_IPP
