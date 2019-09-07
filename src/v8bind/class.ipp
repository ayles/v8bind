//
// Created by selya on 03.09.2019.
//

#ifndef SANDWICH_V8B_CLASS_IPP
#define SANDWICH_V8B_CLASS_IPP

#include <v8bind/class.hpp>
#include <v8bind/function.hpp>

#include <v8.h>

#include <stdexcept>
#include <algorithm>

namespace v8b {

ClassManager::ClassManager(v8::Isolate *isolate, const v8b::TypeInfo &type_info)
        : type_info(type_info), isolate(isolate), auto_wrap(false),
        pointer_auto_wrap(false), constructor_function(nullptr), destructor_function(nullptr) {
    v8::HandleScope scope(isolate);

    auto f = v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value> &args) {
        auto self = ExternalData::Unwrap<ClassManager *>(args.Data());
        try {
            args.GetReturnValue().Set(self->WrapObject(self->constructor_function(args)));
        } catch (const std::exception &e) {
            args.GetIsolate()->ThrowException(v8::Exception::Error(v8_str(e.what())));
        }
    }, ExternalData::New(isolate, this));

    function_template.Reset(isolate, f);

    // 0 - raw pointer to C++ object
    // 1 - pointer to this ClassManager
    f->InstanceTemplate()->SetInternalFieldCount(2);
}

ClassManager::~ClassManager() {
    RemoveObjects();
}

void ClassManager::RemoveObject(void *ptr) {
    auto it = objects.find(ptr);
    if (it == objects.end()) {
        throw std::runtime_error("Can't remove unmanaged object");
    }
    v8::HandleScope scope(isolate);
    ResetObject(it->second);
    objects.erase(it);
}

void ClassManager::RemoveObjects() {
    v8::HandleScope scope(isolate);
    for (auto &p : objects) {
        ResetObject(p.second);
    }
    objects.clear();
}

ClassManager::WrappedObject &ClassManager::FindWrappedObject(void *ptr, void **base_ptr_ptr) const {
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

void ClassManager::ResetObject(WrappedObject &object) {
    if (object.pointer_manager) {
        object.pointer_manager->EndObjectManage(object.ptr);
    }
    object.wrapped_object.Reset();
    isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(type_info.GetSize()));
}

v8::Local<v8::Object> ClassManager::FindObject(void *ptr) const {
    return FindWrappedObject(ptr).wrapped_object.Get(isolate);
}

void ClassManager::SetPointerManager(void *ptr, PointerManager *pointer_manager) {
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
    pointer_manager->BeginObjectManage(ptr);
}

v8::Local<v8::Object> ClassManager::WrapObject(void *ptr) {
    return WrapObject(ptr, this);
}

v8::Local<v8::Object> ClassManager::WrapObject(void *ptr, PointerManager *pointer_manager) {
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

    if (pointer_manager != nullptr) {
        pointer_manager->BeginObjectManage(ptr);
    }

    return scope.Escape(wrapped);
}

void *ClassManager::UnwrapObject(v8::Local<v8::Value> value) {
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

v8::Local<v8::FunctionTemplate> ClassManager::GetFunctionTemplate() const {
    return function_template.Get(isolate);
}

void ClassManager::SetBase(const v8b::TypeInfo &type_info,
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

void ClassManager::SetConstructor(ConstructorFunction constructor_function) {
    this->constructor_function = constructor_function;
}

void ClassManager::SetDestructor(DestructorFunction destructor_function) {
    this->destructor_function = destructor_function;
}

void ClassManager::SetAutoWrap(bool auto_wrap) {
    this->auto_wrap = auto_wrap;
}

void ClassManager::SetPointerAutoWrap(bool auto_wrap) {
    this->pointer_auto_wrap = auto_wrap;
}

bool ClassManager::IsAutoWrapEnabled() const {
    return auto_wrap;
}

bool ClassManager::IsPointerAutoWrapEnabled() const {
    return pointer_auto_wrap;
}

v8::Isolate *ClassManager::GetIsolate() const {
    return isolate;
}

void ClassManager::BeginObjectManage(void *ptr) {
    // TODO
}

void ClassManager::EndObjectManage(void *ptr) {
    destructor_function(isolate, ptr);
}

ClassManager &ClassManagerPool::Get(v8::Isolate *isolate, const TypeInfo &type_info) {
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

void ClassManagerPool::Remove(v8::Isolate *isolate, const v8b::TypeInfo &type_info) {
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

void ClassManagerPool::RemoveAll(v8::Isolate *isolate) {
    RemoveInstance(isolate);
}

std::unordered_map<v8::Isolate *, ClassManagerPool> ClassManagerPool::pools;

ClassManagerPool &ClassManagerPool::GetInstance(v8::Isolate *isolate) {
    return pools[isolate];
}

void ClassManagerPool::RemoveInstance(v8::Isolate *isolate) {
    pools.erase(isolate);
}




template<typename T>
template<typename... Args>
void *Class<T>::ObjectCreate(const v8::FunctionCallbackInfo<v8::Value> &args) {
    return CallConstructor<T, Args...>(args);
}

template<typename T>
void Class<T>::ObjectDestroy(v8::Isolate *isolate, void *ptr) {
    auto obj = static_cast<T *>(ptr);
    delete obj;
}

template<typename T>
Class<T>::Class(v8::Isolate *isolate)
        : class_manager(ClassManagerPool::Get(isolate, TypeInfo::Get<T>())) {
    class_manager.SetDestructor(ObjectDestroy);
}

template<typename T>
template<typename B>
Class<T> &Class<T>::Inherit() {
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
template<typename... Args>
Class<T> &Class<T>::Constructor() {
    class_manager.SetConstructor(ObjectCreate<Args...>);
    return *this;
}

template<typename T>
template<typename Member>
Class<T> &Class<T>::Var(const std::string &name, Member &&ptr) {
    static_assert(std::is_member_object_pointer_v<Member>,
                  "Ptr must be pointer to member data");

    v8::HandleScope scope(class_manager.GetIsolate());

    using MemberTrait = typename utility::FunctionTraits<Member>;

    class_manager.GetFunctionTemplate()->PrototypeTemplate()->SetAccessor(
            ToV8(class_manager.GetIsolate(), name),
            [](v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info) {
                try {
                    auto obj = UnwrapObject(info.GetIsolate(), info.This());
                    auto member = ExternalData::Unwrap<Member>(info.Data());
                    info.GetReturnValue().Set(ToV8(info.GetIsolate(), (*obj).*member));
                } catch (const std::exception &e) {
                    info.GetIsolate()->ThrowException(v8::Exception::Error(v8_str(e.what())));
                }
            },
            [](v8::Local<v8::String> property, v8::Local<v8::Value> value,
                    const v8::PropertyCallbackInfo<void> &info) {
                try {
                    auto obj = UnwrapObject(info.GetIsolate(), info.This());
                    auto member = ExternalData::Unwrap<Member>(info.Data());
                    (*obj).*member = FromV8<typename MemberTrait::ReturnType>(info.GetIsolate(), value);
                } catch (const std::exception &e) {
                    info.GetIsolate()->ThrowException(v8::Exception::Error(v8_str(e.what())));
                }
            },
            ExternalData::New(class_manager.GetIsolate(), std::forward<Member>(ptr))
    );

    return *this;
}

template<typename T>
template<typename ...F>
Class<T> &Class<T>::Function(const std::string &name, F&&... f) {
    static_assert(utility::And(std::is_member_function_pointer_v<F>...),
                  "All f's must be pointers to member functions");

    v8::HandleScope scope(class_manager.GetIsolate());

    class_manager.GetFunctionTemplate()->PrototypeTemplate()->Set(v8_str(name.c_str()),
            WrapFunction(class_manager.GetIsolate(), std::forward<F>(f)...));

    return *this;
}

template<typename T>
Class<T> &Class<T>::AutoWrap(bool auto_wrap) {
    class_manager.SetAutoWrap(auto_wrap);
    return *this;
}

template<typename T>
Class<T> &Class<T>::PointerAutoWrap(bool auto_wrap) {
    class_manager.SetPointerAutoWrap(auto_wrap);
    return *this;
}

template<typename T>
v8::Local<v8::FunctionTemplate> Class<T>::GetFunctionTemplate() {
    return class_manager.GetFunctionTemplate();
}

template<typename T>
T *Class<T>::UnwrapObject(v8::Isolate *isolate, v8::Local<v8::Value> value) {
    return static_cast<T *>(ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).UnwrapObject(value));
}

template<typename T>
v8::Local<v8::Object> Class<T>::WrapObject(v8::Isolate *isolate, T *ptr, bool take_ownership) {
    if (!take_ownership) {
        return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).WrapObject(ptr, nullptr);
    }
    return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).WrapObject(ptr);
}

template<typename T>
v8::Local<v8::Object> Class<T>::WrapObject(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager) {
    return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).WrapObject(ptr, pointer_manager);
}

template<typename T>
void Class<T>::SetPointerManager(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager) {
    ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).SetPointerManager(ptr, pointer_manager);
}

template<typename T>
v8::Local<v8::Object> Class<T>::FindObject(v8::Isolate *isolate, const T &obj) {
    auto &class_manager = ClassManagerPool::Get(isolate, TypeInfo::Get<T>());
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
v8::Local<v8::Object> Class<T>::FindObject(v8::Isolate *isolate, T *ptr) {
    auto &class_manager = ClassManagerPool::Get(isolate, TypeInfo::Get<T>());
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
