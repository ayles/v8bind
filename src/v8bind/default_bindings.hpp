//
// Created by selya on 10.09.2019.
//

#ifndef SANDWICH_V8B_ARRAY_HPP
#define SANDWICH_V8B_ARRAY_HPP

#include <v8bind/class.hpp>

#include <list>
#include <iterator>
#include <type_traits>

namespace v8b {

template<typename T>
struct DefaultBindings {
    static void Initialize(v8::Isolate *) {

    }
};

template<typename T>
struct DefaultBindings<std::vector<T>> {
    static void Initialize(v8::Isolate *isolate) {
        v8b::Class<std::vector<T>> c(isolate);

        c
        .Property("length", &std::vector<T>::size)
        .Indexer([](std::vector<T> &v, uint32_t index) {
            if (index >= v.size()) {
                throw std::out_of_range("Index out of range");
            }
            return v[index];
        }, [](std::vector<T> &v, uint32_t index, const T &value) {

        })
        .Function("push", [](std::vector<T> &v, const T &value) {
            v.emplace_back(value);
            return v.size();
        })
        .Function("pop", [](std::vector<T> &v) {
            if (v.empty()) {
                throw std::out_of_range("Can't pop from empty vector");
            }
            decltype(auto) val = v[v.size() - 1];
            v.resize(v.size() - 1);
            return val;
        })
        /*.Function("splice", [](std::vector<T> &v, uint32_t start_index, uint32_t count) {
            if (start_index + count < v.size()) {
                throw std::out_of_range("Can't splice " +
                    std::to_string(count) + " elements from index " + std::to_string(start_index) +
                    " (length is " + std::to_string(v.size()) + ")");
            }
            auto begin = v.begin();
            std::advance(begin, start_index);
            auto end = begin;
            std::advance(end, count);
            std::vector<T> ret;
            ret.splice(ret.begin(), ret, begin, end);
            return ret;
        })*/
        .Function("toString", [](std::vector<T> &v) {
            return std::string("[native vector of ") + std::to_string(v.size()) + " " + TypeInfo::Get<T>().GetName() + "]";
        })
        .AutoWrap()
        .PointerAutoWrap()
        ;
    }
};

}

#endif //SANDWICH_V8B_ARRAY_HPP
