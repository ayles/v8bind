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
        : type_info(type_info), isolate(isolate), auto_wrap(false), pointer_auto_wrap(false) {
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

void ClassManager::ResetObject(WrappedObject &object) {
    if (object.pointer_manager) {
        object.pointer_manager->EndObjectManage(object.ptr);
    }
    object.wrapped_object.Reset();
}

v8::Local<v8::Object> ClassManager::FindObject(void *ptr) const {
    auto it = objects.find(ptr);
    if (it == objects.end()) {
        throw std::runtime_error("Can't find wrapped object");
    }
    return it->second.wrapped_object.Get(isolate);
}

v8::Local<v8::Object> ClassManager::SetPointerManager(void *ptr, PointerManager *pointer_manager) {
    if (pointer_manager == nullptr) {
        throw std::runtime_error("Can't set nullptr as pointer manager");
    }

    auto it = objects.find(ptr);
    if (it == objects.end()) {
        throw std::runtime_error("Can't find object");
    }

    if (it->second.pointer_manager != nullptr && it->second.pointer_manager != this) {
        throw std::runtime_error("Custom pointer manager already set");
    }

    it->second.pointer_manager = pointer_manager;
    pointer_manager->BeginObjectManage(ptr);

    return it->second.wrapped_object.Get(isolate);
}

v8::Local<v8::Object> ClassManager::WrapObject(void *ptr) {
    return WrapObject(ptr, this);
}

v8::Local<v8::Object> ClassManager::WrapObject(void *ptr, PointerManager *pointer_manager) {
    if (!ptr) {
        return v8::Local<v8::Object>();
    }

    auto it = objects.find(ptr);
    if (it != objects.end()) {
        throw std::runtime_error("Object is already wrapped");
    }

    v8::EscapableHandleScope scope(isolate);

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
    auto self = static_cast<ClassManager *>(obj->GetAlignedPointerFromInternalField(1));

    auto it = self->objects.find(ptr);
    if (it == self->objects.end()) {
        throw std::runtime_error("Can't find object");
    }

    return it->second.ptr;
}

v8::Local<v8::FunctionTemplate> ClassManager::GetFunctionTemplate() const {
    return function_template.Get(isolate);
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
    auto obj = CallConstructor<T, Args...>(args);
    if (!obj) {
        throw std::runtime_error("No suitable constructor found");
    }
    args.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(sizeof(T)));
    return obj;
}

template<typename T>
void Class<T>::ObjectDestroy(v8::Isolate *isolate, void *ptr) {
    auto obj = static_cast<T *>(ptr);
    delete obj;
    isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(sizeof(T)));
}

template<typename T>
Class<T>::Class(v8::Isolate *isolate)
        : class_manager(ClassManagerPool::Get(isolate, TypeInfo::Get<T>())) {
    class_manager.SetDestructor(ObjectDestroy);
}

template<typename T>
template<typename... Args>
Class<T> &Class<T>::Constructor() {
    class_manager.SetConstructor(ObjectCreate<Args...>);
    return *this;
}

template<typename T>
template<typename Member>
Class<T> &Class<T>::Var(const std::string &name, Member ptr) {
    static_assert(std::is_member_object_pointer_v<Member>,
                  "Ptr must be pointer to member data");

    v8::HandleScope scope(class_manager.GetIsolate());

    using MemberTrait = typename utility::FunctionTraits<Member>;
    using MemberPointer = typename MemberTrait::PointerType;
    MemberPointer member = ptr;

    class_manager.GetFunctionTemplate()->PrototypeTemplate()->SetAccessor(
            ToV8(class_manager.GetIsolate(), name),
            [](Local<String> property, const v8::PropertyCallbackInfo<Value>& info) {
                try {
                    auto obj = UnwrapObject(info.GetIsolate(), info.This());
                    auto member = ExternalData::Unwrap<MemberPointer>(info.Data());
                    info.GetReturnValue().Set(ToV8(info.GetIsolate(), (*obj).*member));
                } catch (const std::exception &e) {
                    info.GetIsolate()->ThrowException(v8::Exception::Error(v8_str(e.what())));
                }
            },
            [](Local<String> property, Local<Value> value,
                    const v8::PropertyCallbackInfo<void>& info) {
                try {
                    auto obj = UnwrapObject(info.GetIsolate(), info.This());
                    auto member = ExternalData::Unwrap<MemberPointer>(info.Data());
                    (*obj).*member = FromV8<MemberTrait::ReturnType>(info.GetIsolate(), value);
                } catch (const std::exception &e) {
                    info.GetIsolate()->ThrowException(v8::Exception::Error(v8_str(e.what())));
                }
            },
            ExternalData::New(class_manager.GetIsolate(), member)
    );

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
        return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).WrapObject(ptr, nullptr)
    }
    return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).WrapObject(ptr);
}

template<typename T>
v8::Local<v8::Object> Class<T>::WrapObject(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager) {
    return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).WrapObject(ptr, pointer_manager);
}

template<typename T>
v8::Local<v8::Object> Class<T>::SetPointerManager(v8::Isolate *isolate, T *ptr, PointerManager *pointer_manager) {
    return ClassManagerPool::Get(isolate, TypeInfo::Get<T>()).SetPointerManager(ptr, pointer_manager);
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
        // Do not manage auto wrapped pointers
        // They later can be managed with PointerManager
        return class_manager.WrapObject(ptr, nullptr);
    }
}


}

#endif //SANDWICH_V8B_CLASS_IPP
