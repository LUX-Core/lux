cmake_minimum_required(VERSION 3.10.3)
project(lux)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_custom_target(build-lux ALL
        COMMAND ./autogen.sh
        COMMAND ./configure
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $(MAKE))

add_custom_target(build-luxgate-node ALL
        COMMAND ./autogen.sh
        COMMAND ./configure --disable-tests --without-gui --enable-luxgate --enable-luxgate-tests
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $(MAKE))

add_custom_target(build-luxgate ALL
        COMMAND ./autogen.sh
        COMMAND ./configure --disable-tests --enable-luxgate --enable-luxgate-tests
        COMMAND $(MAKE) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $(MAKE))

add_custom_target(debug-luxd ALL)
add_custom_target(debug-lux-qt ALL)
add_custom_target(debug-luxgate-tests ALL)