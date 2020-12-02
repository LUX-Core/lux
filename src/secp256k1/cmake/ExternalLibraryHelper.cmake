include(FindPackageMessage)

# Find a library component, set the variables and create an imported target.
# Variable names are compliant with cmake standards.
function(find_component LIB COMPONENT)
	cmake_parse_arguments(ARG
		""
		""
		"HINTS;INCLUDE_DIRS;INTERFACE_LINK_LIBRARIES;NAMES;PATHS;PATH_SUFFIXES"
		${ARGN}
	)

	# If the component is not requested, skip the search.
	if(${LIB}_FIND_COMPONENTS AND NOT ${COMPONENT} IN_LIST ${LIB}_FIND_COMPONENTS)
		return()
	endif()

	find_library(${LIB}_${COMPONENT}_LIBRARY
		NAMES ${ARG_NAMES}
		PATHS "" ${ARG_PATHS}
		HINTS "" ${ARG_HINTS}
		PATH_SUFFIXES "" ${ARG_PATH_SUFFIXES}
	)
	mark_as_advanced(${LIB}_${COMPONENT}_LIBRARY)

	if(${LIB}_${COMPONENT}_LIBRARY)
		# On success, set the standard FOUND variable...
		set(${LIB}_${COMPONENT}_FOUND TRUE PARENT_SCOPE)

		# ... and append the library path to the LIBRARIES variable ...
		list(APPEND ${LIB}_LIBRARIES
			"${${LIB}_${COMPONENT}_LIBRARY}"
			${ARG_INTERFACE_LINK_LIBRARIES}
		)
		list(REMOVE_DUPLICATES ${LIB}_LIBRARIES)
		set(${LIB}_LIBRARIES ${${LIB}_LIBRARIES} PARENT_SCOPE)

		# ... and create an imported target for the component, if not already
		# done.
		if(NOT TARGET ${LIB}::${COMPONENT})
			add_library(${LIB}::${COMPONENT} UNKNOWN IMPORTED)
			set_target_properties(${LIB}::${COMPONENT} PROPERTIES
				IMPORTED_LOCATION "${${LIB}_${COMPONENT}_LIBRARY}"
			)
			set_property(TARGET ${LIB}::${COMPONENT} PROPERTY
				INTERFACE_INCLUDE_DIRECTORIES ${ARG_INCLUDE_DIRS}
			)
			set_property(TARGET ${LIB}::${COMPONENT} PROPERTY
				INTERFACE_LINK_LIBRARIES ${ARG_INTERFACE_LINK_LIBRARIES}
			)
		endif()

		find_package_message("${LIB}_${COMPONENT}"
			"Found ${LIB} component ${COMPONENT}: ${${LIB}_${COMPONENT}_LIBRARY}"
			"[${${LIB}_${COMPONENT}_LIBRARY}][${ARG_INCLUDE_DIRS}]"
		)
	endif()
endfunction()
