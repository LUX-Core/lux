# Copyright (c) 2017 The Bitcoin developers

project(zmq)
cmake_minimum_required(VERSION 3.16)

#ADDED FOR TRANSITION PURPOSES
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
add_definitions(-DENABLE_ZMQ -DHAVE_CONFIG_H)
include_directories(".")
include_directories("../")
include_directories("../cpp-ethereum/")
include_directories("../leveldb/include/")
#END_ADDED
add_library(zmq
	zmqabstractnotifier.cpp
	zmqnotificationinterface.cpp
	zmqpublishnotifier.cpp
)

find_package(ZeroMQ 4.1.5 REQUIRED)
target_link_libraries(zmq util ZeroMQ::zmq)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_compile_definitions(zmq PUBLIC ZMQ_STATIC)
endif()
