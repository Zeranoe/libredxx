cmake_minimum_required(VERSION 3.16)

project(libredxx C)

option(LIBREDXX_ENABLE_EXAMPLES "Build examples" OFF)
option(LIBREDXX_COMPILE_WARNING_AS_ERROR "Treat compile warnings as errors" OFF)
option(LIBREDXX_DISABLE_INSTALL "Disable install rules" OFF)

add_subdirectory(libredxx)

if(LIBREDXX_ENABLE_EXAMPLES)
    add_subdirectory(example)
endif()
