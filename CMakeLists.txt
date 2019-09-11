cmake_minimum_required(VERSION 3.13)
project(V8Bind)

set(CMAKE_CXX_STANDARD 17)

set(V8BIND_HEADERS
        src/v8bind/class.hpp
        src/v8bind/traits.hpp
        src/v8bind/convert.hpp
        src/v8bind/function.hpp
        src/v8bind/type_info.hpp
        src/v8bind/class.ipp
        src/v8bind/default_bindings.hpp
        src/v8bind/v8bind.hpp
        src/v8bind/module.hpp
        src/v8bind/property.hpp)

set(V8BIND_SOURCES
        src/v8bind/stub.cpp)

if (NOT DEFINED V8_INCLUDE_DIR)
    message(FATAL_ERROR "V8_INCLUDE_DIRECTORY not set")
endif ()

add_library(v8bind STATIC ${V8BIND_HEADERS} ${V8BIND_SOURCES})
target_include_directories(v8bind PUBLIC src ${V8_INCLUDE_DIR})