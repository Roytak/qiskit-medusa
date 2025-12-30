# FindSylvan.cmake
# - Tries to find the Sylvan library and headers
#
# This module defines the following variables:
#  SYLVAN_FOUND        - True if Sylvan was found
#  SYLVAN_INCLUDE_DIRS - Path to the Sylvan include directory
#  SYLVAN_LIBRARIES    - The Sylvan library to link against
#
# It also defines the following IMPORTED target:
#  Sylvan::Sylvan      - The target to link against (automatically handles include directories)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_SYLVAN QUIET sylvan)
endif()

# 1. Find the Sylvan Header
find_path(SYLVAN_INCLUDE_DIR
    NAMES sylvan.h
    HINTS ${PC_SYLVAN_INCLUDEDIR} ${PC_SYLVAN_INCLUDE_DIRS}
    PATH_SUFFIXES include
    DOC "Path to the Sylvan header files"
)

# 2. Find the Sylvan Library
find_library(SYLVAN_LIBRARY
    NAMES sylvan
    HINTS ${PC_SYLVAN_LIBDIR} ${PC_SYLVAN_LIBRARY_DIRS}
    PATH_SUFFIXES lib
    DOC "Path to the Sylvan library"
)

# 3. Handle 'Lace' dependency if Sylvan was built statically or separate
# Sylvan depends on 'Lace'. If it's not bundled, we might need to find it too.
# This step is optional but helpful for static builds.
find_library(SYLVAN_LACE_LIBRARY
    NAMES lace
    HINTS ${PC_SYLVAN_LIBDIR} ${PC_SYLVAN_LIBRARY_DIRS}
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sylvan
    REQUIRED_VARS SYLVAN_LIBRARY SYLVAN_INCLUDE_DIR
    HANDLE_COMPONENTS
)

if(SYLVAN_FOUND)
    set(SYLVAN_INCLUDE_DIRS ${SYLVAN_INCLUDE_DIR})
    set(SYLVAN_LIBRARIES ${SYLVAN_LIBRARY})

    # If Lace was found, append it to libraries
    if(SYLVAN_LACE_LIBRARY)
        list(APPEND SYLVAN_LIBRARIES ${SYLVAN_LACE_LIBRARY})
    endif()

    if(NOT TARGET Sylvan::Sylvan)
        add_library(Sylvan::Sylvan UNKNOWN IMPORTED)
        set_target_properties(Sylvan::Sylvan PROPERTIES
            IMPORTED_LOCATION "${SYLVAN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SYLVAN_INCLUDE_DIR}"
        )
        # Link Lace to the imported target if found
        if(SYLVAN_LACE_LIBRARY)
            set_target_properties(Sylvan::Sylvan PROPERTIES
                INTERFACE_LINK_LIBRARIES "${SYLVAN_LACE_LIBRARY}"
            )
        endif()
    endif()
endif()

mark_as_advanced(SYLVAN_INCLUDE_DIR SYLVAN_LIBRARY SYLVAN_LACE_LIBRARY)
