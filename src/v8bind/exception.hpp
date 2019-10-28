//
// Created by selya on 28.10.2019.
//

#ifndef SANDWICH_V8B_EXCEPTION_HPP
#define SANDWICH_V8B_EXCEPTION_HPP

#include <stdexcept>
#include <string>

class V8BindException : public std::runtime_error {
public:
    explicit V8BindException(const std::string &cause) : std::runtime_error(cause) {}
};

class CallException : public V8BindException {
public:
    explicit CallException(const std::string &cause) : V8BindException(cause) {}
};

#endif //SANDWICH_V8B_EXCEPTION_HPP
