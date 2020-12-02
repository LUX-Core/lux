# Copyright (c) 2017-2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# .rst:
# FindZeroMQ
# --------------
#
# Find the ZeroMQ library. The following conponents are
# available::
#   zmq
#
# This will define the following variables::
#
#   ZeroMQ_FOUND - True if the ZeroMQ library is found.
#   ZeroMQ_INCLUDE_DIRS - List of the header include directories.
#   ZeroMQ_LIBRARIES - List of the libraries.
#   ZeroMQ_VERSION - The library version MAJOR.MINOR.PATCH
#   ZeroMQ_VERSION_MAJOR - Major version number
#   ZeroMQ_VERSION_MINOR - Minor version number
#   ZeroMQ_VERSION_PATCH - Patch version number
#
# And the following imported targets::
#
#   ZeroMQ::zmq

find_path(ZeroMQ_INCLUDE_DIR
	NAMES zmq.h
)

set(ZeroMQ_INCLUDE_DIRS "${ZeroMQ_INCLUDE_DIR}")
mark_as_advanced(ZeroMQ_INCLUDE_DIR)

if(ZeroMQ_INCLUDE_DIR)
	# Extract version information from the zmq.h header.
	if(NOT DEFINED ZeroMQ_VERSION)
		# Read the version from file zmq.h into a variable.
		file(READ "${ZeroMQ_INCLUDE_DIR}/zmq.h" _ZMQ_HEADER)

		# Parse the version into variables.
		string(REGEX REPLACE
			".*ZMQ_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1"
			ZeroMQ_VERSION_MAJOR
			"${_ZMQ_HEADER}"
		)
		string(REGEX REPLACE
			".*ZMQ_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
			ZeroMQ_VERSION_MINOR
			"${_ZMQ_HEADER}"
		)
		# Patch version example on non-crypto installs: x.x.xNC
		string(REGEX REPLACE
			".*ZMQ_VERSION_PATCH[ \t]+([0-9]+(NC)?).*" "\\1"
			ZeroMQ_VERSION_PATCH
			"${_ZMQ_HEADER}"
		)

		# Cache the result.
		set(ZeroMQ_VERSION_MAJOR ${ZeroMQ_VERSION_MAJOR}
			CACHE INTERNAL "ZeroMQ major version number"
		)
		set(ZeroMQ_VERSION_MINOR ${ZeroMQ_VERSION_MINOR}
			CACHE INTERNAL "ZeroMQ minor version number"
		)
		set(ZeroMQ_VERSION_PATCH ${ZeroMQ_VERSION_PATCH}
			CACHE INTERNAL "ZeroMQ patch version number"
		)
		# The actual returned/output version variable (the others can be used if
		# needed).
		set(ZeroMQ_VERSION
			"${ZeroMQ_VERSION_MAJOR}.${ZeroMQ_VERSION_MINOR}.${ZeroMQ_VERSION_PATCH}"
			CACHE INTERNAL "ZeroMQ full version"
		)
	endif()

	include(ExternalLibraryHelper)

	# The dependency to iphlpapi starts from 4.2.0
	if(ZeroMQ_VERSION VERSION_LESS 4.2.0)
		set(_ZeroMQ_WINDOWS_LIBRARIES "$<$<PLATFORM_ID:Windows>:ws2_32;rpcrt4>")
	else()
		set(_ZeroMQ_WINDOWS_LIBRARIES "$<$<PLATFORM_ID:Windows>:ws2_32;rpcrt4;iphlpapi>")
	endif()

	find_component(ZeroMQ zmq
		NAMES zmq
		INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIRS}
		INTERFACE_LINK_LIBRARIES "${_ZeroMQ_WINDOWS_LIBRARIES}"
	)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZeroMQ
	REQUIRED_VARS ZeroMQ_INCLUDE_DIR
	VERSION_VAR ZeroMQ_VERSION
	HANDLE_COMPONENTS
)
