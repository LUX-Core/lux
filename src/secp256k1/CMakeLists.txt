# Copyright (c) 2017 The Bitcoin developers

cmake_minimum_required(VERSION 3.5)
project(secp256k1)

# libsecp256k1 use a different set of flags.
#add_compiler_flags(
#	-pedantic
#	-Wshadow
#	-Wno-unused-function
#	-Wno-nonnull
#	-Wno-overlength-strings
#)

#add_c_compiler_flags(
#	-std=c89
#	-Wno-long-long
#)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c89 -Wno-long-long")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wshadow -Wno-unused-function -Wno-nonnull -Wno-overlength-strings")


# Default visibility is hidden on all targets.
set(CMAKE_C_VISIBILITY_PRESET hidden)

include_directories(
	.
	src
	# For the config
	${CMAKE_CURRENT_BINARY_DIR}/src
)

# The library
add_library(secp256k1 src/secp256k1.c)
target_include_directories(secp256k1 PUBLIC include)

# We need to link in GMP
find_package(GMP)
if(GMP_FOUND)
	target_include_directories(secp256k1 PUBLIC ${GMP_INCLUDE_DIR})
	target_link_libraries(secp256k1 ${GMP_LIBRARIES})
	set(USE_NUM_GMP 1)
	set(USE_FIELD_INV_NUM 1)
	set(USE_SCALAR_INV_NUM 1)
else()
	set(USE_NUM_NONE 1)
	set(USE_FIELD_INV_BUILTIN 1)
	set(USE_SCALAR_INV_BUILTIN 1)
endif()

# We check if amd64 asm is supported.
#check_c_source_compiles("
#	#include <stdint.h>
#	int main() {
#		uint64_t a = 11, tmp;
#		__asm__ __volatile__(\"movq \$0x100000000,%1; mulq %%rsi\" : \"+a\"(a) : \"S\"(tmp) : \"cc\", \"%rdx\");
#		return 0;
#	}
#" USE_ASM_X86_64)

# We make sure __int128 is defined
include(CheckTypeSize)
check_type_size(__int128 SIZEOF___INT128)
if(SIZEOF___INT128 EQUAL 16)
	set(HAVE___INT128 1)
else()
	# If we do not support __int128, we should be falling back
	# on 32bits implementations for field and scalar.
endif()

# Detect if we are on a 32 or 64 bits plateform and chose
# scalar and filed implementation accordingly
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	# 64 bits implementationr require either __int128 or asm support.
	if (HAVE___INT128 OR USE_ASM_X86_64)
		set(USE_SCALAR_4X64 1)
		set(USE_FIELD_5X52 1)
	else()
		message(SEND_ERROR "Compiler does not support __int128 or insline assembly")
	endif()
else()
	set(USE_SCALAR_8X32 1)
	set(USE_FIELD_10X26 1)
endif()

# Executable internal to secp256k1 need to have the HAVE_CONFIG_H define set.
# For convenience, we wrap this into a function.
function(link_secp256k1_internal NAME)
	target_link_libraries(${NAME} secp256k1)
	target_compile_definitions(${NAME} PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)
endfunction(link_secp256k1_internal)

# Phony target to build benchmarks
add_custom_target(bench-secp256k1)

function(add_secp256k1_bench NAME)
	add_executable(${NAME} EXCLUDE_FROM_ALL ${ARGN})
	link_secp256k1_internal(${NAME})
	add_dependencies(bench-secp256k1 ${NAME})
endfunction(add_secp256k1_bench)

# ECDH module
option(SECP256K1_ENABLE_MODULE_ECDH "Build libsecp256k1's ECDH module" OFF)
#add_feature_info(WITH_ECHD_MODULE SECP256K1_ENABLE_MODULE_ECDH "Build libsecp256k1's ECDH module")
if(SECP256K1_ENABLE_MODULE_ECDH)
	set(ENABLE_MODULE_ECDH 1)
	add_secp256k1_bench(bench_ecdh src/bench_ecdh.c)
endif()

# Recovery module
option(SECP256K1_ENABLE_MODULE_RECOVERY "Build libsecp256k1's recovery module" ON)
#add_feature_info(WITH_MODULE_RECOVERY SECP256K1_ENABLE_MODULE_RECOVERY "Build libsecp256k1's recovery module")
if(SECP256K1_ENABLE_MODULE_RECOVERY)
	set(ENABLE_MODULE_RECOVERY 1)
	add_secp256k1_bench(bench_recover src/bench_recover.c)
endif()

# Schnorr module
option(SECP256K1_ENABLE_MODULE_SCHNORR "Build libsecp256k1's Schnorr module" OFF)
#add_feature_info(WITH_SCHNORR SECP256K1_ENABLE_MODULE_SCHNORR "Build libsecp256k1's Schnorr module")
if(SECP256K1_ENABLE_MODULE_SCHNORR)
	set(ENABLE_MODULE_SCHNORR 1)
endif()

# Generate the config
configure_file(src/libsecp256k1-config.h.cmake.in src/libsecp256k1-config.h ESCAPE_QUOTES)
target_compile_definitions(secp256k1 PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)

# Tests
option(SECP256K1_BUILD_TEST "Build secp256k1's unit tests" OFF)
#add_feature_info(WITH_SECP256K1_TESTS SECP256K1_BUILD_TEST "Build secp256k1's unit tests")
if(SECP256K1_BUILD_TEST)
	include(TestSuite)
	create_test_suite(secp256k1)

	function(create_secp256k1_test NAME FILES)
		add_test_to_suite(secp256k1 ${NAME} EXCLUDE_FROM_ALL ${FILES})
		link_secp256k1_internal(${NAME})
	endfunction()

	create_secp256k1_test(tests src/tests.c)
	target_compile_definitions(tests PRIVATE VERIFY)

	create_secp256k1_test(exhaustive_tests src/tests_exhaustive.c)
	# This should not be enabled at the same time as coverage is.
	# TODO: support coverage.
	target_compile_definitions(exhaustive_tests PRIVATE VERIFY)
endif(SECP256K1_BUILD_TEST)

# Benchmarks
option(SECP256K1_BUILD_BENCH "Build secp256k1's benchmarks" OFF)
#add_feature_info(WITH_SECP256K1_BENCH SECP256K1_BUILD_BENCH "Build secp256k1's benchmarks")
if(SECP256K1_BUILD_BENCH)
  add_secp256k1_bench(bench_verify src/bench_verify.c)
  add_secp256k1_bench(bench_sign src/bench_sign.c)
  add_secp256k1_bench(bench_internal src/bench_internal.c)
endif()
