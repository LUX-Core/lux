file(GLOB SRC_LIST "*.cpp")
file(GLOB HEADERS "*.h")

add_library(cpp-ethereum ${SRC_LIST} ${HEADERS})

target_link_libraries(cpp-ethereum ${Boost_REGEX_LIBRARIES} JsonCpp)

find_package(Dev)
find_package(Eth)

target_include_directories(cpp-ethereum PRIVATE ..)
target_include_directories(cpp-ethereum PRIVATE ../utils)
target_link_libraries(cpp-ethereum ${Eth_ETHCORE_LIBRARIES})
target_link_libraries(cpp-ethereum ${Eth_EVM_LIBRARIES})

if (NOT EMSCRIPTEN)
	target_link_libraries(cpp-ethereum ${Dev_P2P_LIBRARIES})
	target_link_libraries(cpp-ethereum ${Dev_DEVCRYPTO_LIBRARIES})
endif()

if (EVMJIT)
	target_link_libraries(cpp-ethereum ${Eth_EVMJIT_LIBRARIES})
endif()
