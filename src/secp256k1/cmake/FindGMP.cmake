# Copyright (c) 2017-2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#.rst
# FindGMP
# -------------
#
# Find the GNU Multiple Precision Arithmetic library. The following components
# are available::
#   gmp
#
# This will define the following variables::
#
#   GMP_FOUND - system has GMP lib
#   GMP_INCLUDE_DIRS - the GMP include directories
#   GMP_LIBRARIES - Library needed to use GMP
#   GMP_VERSION - The library version MAJOR.MINOR.PATCH
#   GMP_VERSION_MAJOR - The library MAJOR version number
#   GMP_VERSION_MINOR - The library MINOR version number
#   GMP_VERSION_PATCH - The library PATCH version number
#
# And the following imported target::
#
#   GMP::gmp

include(BrewHelper)
find_brew_prefix(BREW_HINT gmp)

find_path(GMP_INCLUDE_DIR
	NAMES gmp.h
	HINTS ${BREW_HINT}
)
set(GMP_INCLUDE_DIRS "${GMP_INCLUDE_DIR}")
mark_as_advanced(GMP_INCLUDE_DIR)

if(GMP_INCLUDE_DIR)
	# Extract version information from the gmp.h header.
	if(NOT DEFINED GMP_VERSION)
		# Read the version from file gmp.h into a variable.
		file(READ "${GMP_INCLUDE_DIR}/gmp.h" _GMP_HEADER)

		# Parse the version into variables.
		string(REGEX REPLACE
			".*__GNU_MP_VERSION[ \t]+([0-9]+).*" "\\1"
			GMP_VERSION_MAJOR
			"${_GMP_HEADER}"
		)
		string(REGEX REPLACE
			".*__GNU_MP_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
			GMP_VERSION_MINOR
			"${_GMP_HEADER}"
		)
		string(REGEX REPLACE
			".*__GNU_MP_VERSION_PATCHLEVEL[ \t]+([0-9]+).*" "\\1"
			GMP_VERSION_PATCH
			"${_GMP_HEADER}"
		)

		# Cache the result.
		set(GMP_VERSION_MAJOR ${GMP_VERSION_MAJOR}
			CACHE INTERNAL "GMP major version number"
		)
		set(GMP_VERSION_MINOR ${GMP_VERSION_MINOR}
			CACHE INTERNAL "GMP minor version number"
		)
		set(GMP_VERSION_PATCH ${GMP_VERSION_PATCH}
			CACHE INTERNAL "GMP patch version number"
		)
		# The actual returned/output version variable (the others can be used if
		# needed).
		set(GMP_VERSION
			"${GMP_VERSION_MAJOR}.${GMP_VERSION_MINOR}.${GMP_VERSION_PATCH}"
			CACHE INTERNAL "GMP full version"
		)
	endif()

	include(ExternalLibraryHelper)
	find_component(GMP gmp
		NAMES gmp
		HINTS ${BREW_HINT}
		INCLUDE_DIRS ${GMP_INCLUDE_DIRS}
	)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP
	REQUIRED_VARS GMP_INCLUDE_DIR
	VERSION_VAR GMP_VERSION
	HANDLE_COMPONENTS
)
