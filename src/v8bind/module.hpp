//
// Created by selya on 11.09.2019.
//

#ifndef SANDWICH_V8B_MODULE_HPP
#define SANDWICH_V8B_MODULE_HPP

#include <v8bind/class.hpp>
#include <v8bind/convert.hpp>
#include <v8bind/function.hpp>
#include <v8bind/property.hpp>

#include <v8.h>

#include <cstddef>

namespace v8b {

class Module {
public:
    explicit Module(v8::Isolate *isolate) : isolate(isolate) {
        object.Reset(isolate, v8::ObjectTemplate::New(isolate));
    }

    template<typename Data>
    Module &Value(const std::string &name, v8::Local<Data> value,
            v8::PropertyAttribute attribute = v8::None) {
        object.Get(isolate)->Set(ToV8(isolate, name), value, attribute);
        return *this;
    }

    template<typename T>
    Module &Class(const std::string &name, const v8b::Class<T> &cl) {
        cl.GetFunctionTemplate()->SetClassName(ToV8(isolate, name));
        return Value(name, cl.GetFunctionTemplate(),
                v8::PropertyAttribute(v8::DontDelete | v8::ReadOnly));
    }

    Module &Submodule(const std::string &name, Module &module) {
        return Value(name, module.object.Get(module.isolate));
    }

    template<typename V>
    Module &Var(const std::string &name, V &&v) {
        auto data = impl::VarAccessor<false>(isolate, std::forward<V>(v));
        object.Get(isolate)->SetAccessor(ToV8(isolate, name),
                data.getter, data.setter, data.data, v8::DEFAULT, data.attribute);
        return *this;
    }

    template<typename Getter, typename Setter = std::nullptr_t>
    Module &Property(const std::string &name, Getter &&get, Setter &&set = nullptr) {
        auto data = impl::PropertyAccessor<false, Getter, Setter>(
                isolate, std::forward<Getter>(get), std::forward<Setter>(set));
        object.Get(isolate)->SetAccessor(ToV8(isolate, name),
                data.getter, data.setter, data.data, v8::DEFAULT, data.attribute);
        return *this;
    }

    template<typename ...F>
    Module &Function(const std::string &name, F&&... f) {
        return Value(name, WrapFunction<StaticCall>(isolate, std::forward<F>(f)...));
    }

    v8::Local<v8::Object> NewInstance() const {
        return object.Get(isolate)->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
    }

    v8::Local<v8::ObjectTemplate> GetObjectTemplate() const {
        return object.Get(isolate);
    }

private:
    v8::Isolate *isolate;
    v8::Persistent<v8::ObjectTemplate> object;
};

}

#endif //SANDWICH_V8B_MODULE_HPP
