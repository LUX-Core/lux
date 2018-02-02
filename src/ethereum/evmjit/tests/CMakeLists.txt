# Compile tests for headers:
add_library(test-evm-header-cpp STATIC test-evm.cpp)
target_include_directories(test-evm-header-cpp PRIVATE ../include)


# Library tests:
add_executable(test-evmjit-standalone test-evmjit-standalone.c)
target_include_directories(test-evmjit-standalone PRIVATE ../include)
if (MSVC)
    set(SYSTEM_LIBS "")
elseif (APPLE)
    set(SYSTEM_LIBS stdc++ pthread)
else()
    set(SYSTEM_LIBS stdc++ pthread dl m)
endif()
target_link_libraries(test-evmjit-standalone PRIVATE ${EVMJIT_STANDALONE_LIB} ${SYSTEM_LIBS})
add_dependencies(test-evmjit-standalone evmjit-standalone)


add_test(evmjit-standalone test-evmjit-standalone)
