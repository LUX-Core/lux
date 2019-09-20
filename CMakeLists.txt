cmake_minimum_required(VERSION 3.10.3)
project(lux)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_custom_target(build-lux ALL
        COMMAND ./autogen.sh
        COMMAND ./configure --disable-debug
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $(MAKE))

add_custom_target(debug-luxd ALL
        COMMAND ./autogen.sh
        COMMAND ./configure --enable-debug --without-gui
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $(MAKE))

add_custom_target(debug-lux-qt ALL
        COMMAND ./autogen.sh
        COMMAND ./configure --enable-debug --with-gui
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $(MAKE))

include(CLion.cmake)
