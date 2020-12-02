# Copyright (c) 2017-2019 The Bitcoin developers

project(crypto)
cmake_minimum_required(VERSION 3.16)

#ADDED FOR TRANSITION PURPOSES
include_directories(".")
include_directories("../")
include_directories("../cpp-ethereum")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")
#END_ADDED

# The library
add_library(crypto
	hmac_sha512.cpp
	sha256.cpp
	sha1.cpp
	ripemd160.cpp          
	sha512.cpp
	hmac_sha256.cpp
	rfc6979_hmac_sha256.cpp
	scrypt.cpp
        sha256_sse4.cpp
	fugue.c
	gost.c
	jh.c
	skein.c
	cubehash.c
	aes_helper.c
	echo.c
)

target_include_directories(crypto
	PRIVATE
		..
	PUBLIC
		# To access the config.
		${CMAKE_CURRENT_BINARY_DIR}/..
)

target_compile_definitions(crypto PUBLIC HAVE_CONFIG_H)

# Use assembly is specified
option(CRYPTO_USE_ASM "Use assembly version of crypto primitives" ON)
if(CRYPTO_USE_ASM)
	target_compile_definitions(crypto PRIVATE USE_ASM)
endif()

# Crypto libraries requiring hardware features
macro(add_crypto_library NAME)
	add_library(${NAME} ${ARGN})
	target_link_libraries(crypto ${NAME})
	target_include_directories(${NAME} PRIVATE ..)
endmacro()
