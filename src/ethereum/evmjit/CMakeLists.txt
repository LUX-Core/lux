cmake_minimum_required(VERSION 3.4.0)

cmake_policy(SET CMP0042 OLD)	# Fix MACOSX_RPATH.
cmake_policy(SET CMP0048 NEW)	# Allow VERSION argument in project().
if (POLICY CMP0054)
	cmake_policy(SET CMP0054 NEW)	# No longer implicitly dereference variables.
endif()

project(EVMJIT VERSION 0.9.0.2 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

message(STATUS "EVM JIT ${EVMJIT_VERSION_MAJOR}.${EVMJIT_VERSION_MINOR}.${EVMJIT_VERSION_PATCH}")

if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
	message(FATAL_ERROR "Target ${CMAKE_SYSTEM_PROCESSOR} not supported -- EVM JIT works only on x86_64 architecture")
endif()

option(EVMJIT_INCLUDE_EXAMPLES "Generate build targets for the EVMJIT examples" OFF)
option(EVMJIT_TESTS "Create targets for CTest" OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_AUTOMOC OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Always use Release variant of C++ runtime.
	# We don't want to provide Debug variants of all dependencies. Some default
	# flags set by CMake must be tweaked.
	string(REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	string(REPLACE "/D_DEBUG" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS OFF)
else()
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -Wconversion -Wno-sign-conversion -Wno-unknown-pragmas ${CMAKE_CXX_FLAGS}")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT SANITIZE)
	# Do not allow unresolved symbols in shared library (default on linux)
	# unless sanitizer is used (sanity checks produce unresolved function calls)
	set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
endif()

include(ProjectLLVM)
configure_llvm_project()

add_subdirectory(libevmjit)

if (EVMJIT_INCLUDE_EXAMPLES)
	add_subdirectory(examples)
endif()

if (EVMJIT_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
