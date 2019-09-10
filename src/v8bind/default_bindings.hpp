//
// Created by selya on 10.09.2019.
//

#ifndef SANDWICH_V8B_ARRAY_HPP
#define SANDWICH_V8B_ARRAY_HPP

#include <v8bind/class.hpp>

#include <list>
#include <iterator>

namespace v8b {

template<typename T>
struct DefaultBindings {
    static void Initialize(v8::Isolate *) {

    }
};

template<typename T>
struct DefaultBindings<std::list<T>> {
    static void Initialize(v8::Isolate *isolate) {
        v8b::Class<std::list<T>> c(isolate);
        c
        .Property("length", [](std::list<T> &v) {
            return v.size();
        })
        .Indexer([](std::list<T> &v, uint32_t index) {
            if (index >= v.size()) {
                throw std::out_of_range("Index out of range");
            }
            auto it = v.begin();
            std::advance(it, index);
            return *it;
        }, [](std::list<T> &v, uint32_t index, const T &value) {
            if (index >= v.size()) {
                throw std::out_of_range("Index out of range");
            }
            auto it = v.begin();
            std::advance(it, index);
            *it = value;
        })
        .Function("push", [](std::list<T> &v, const T &value) {
            v.emplace_back(value);
            return v.size();
        })
        .Function("pop", [](std::list<T> &v) {
            if (v.empty()) {
                throw std::out_of_range("Can't pop from empty list");
            }
            return *std::prev(v.end());
        })
        .Function("splice", [](std::list<T> &v, uint32_t start_index, uint32_t count) {
            if (start_index + count < v.size()) {
                throw std::out_of_range("Can't splice " +
                    std::to_string(count) + " elements from index " + std::to_string(start_index) +
                    " (length is " + std::to_string(v.size()) + ")");
            }
            auto begin = v.begin();
            std::advance(begin, start_index);
            auto end = begin;
            std::advance(end, count);
            std::list<T> ret;
            ret.splice(ret.begin(), ret, begin, end);
            return ret;
        })
        .Function("toString", [](std::list<T> &v) {
            return std::string("[list of ") + std::to_string(v.size()) + " " + TypeInfo::Get<T>().GetName() + "]";
        })
        .AutoWrap()
        .PointerAutoWrap()
        ;
    }
};

}

#endif //SANDWICH_V8B_ARRAY_HPP
