cmake_minimum_required(VERSION 3.13)
project(perpetual-contracts)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(contracts
        account.cpp
        myTypes.hpp
        order.cpp
        perpetual-swap.cpp)
