# FindGMP.cmake
#
# Finds the GNU Multiple Precision Arithmetic Library (GMP)
#
# This will define the following variables:
#   GMP_FOUND        - True if the system has the GMP library
#   GMP_INCLUDE_DIRS - Include directories needed to use GMP
#   GMP_LIBRARIES    - Libraries needed to link against GMP
#   GMP_VERSION      - The version of GMP found
#
# and the following imported targets:
#   GMP::GMP - The GMP library

find_path(GMP_INCLUDE_DIR
    NAMES gmp.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        /opt/homebrew/include
    DOC "GMP include directory"
)

find_library(GMP_LIBRARY
    NAMES gmp libgmp
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /opt/homebrew/lib
    DOC "GMP library"
)

# Extract version information from gmp.h if found
if(GMP_INCLUDE_DIR AND EXISTS "${GMP_INCLUDE_DIR}/gmp.h")
    file(STRINGS "${GMP_INCLUDE_DIR}/gmp.h" GMP_VERSION_LINE
         REGEX "^#define[ \t]+__GNU_MP_VERSION[ \t]+[0-9]+")
    file(STRINGS "${GMP_INCLUDE_DIR}/gmp.h" GMP_VERSION_MINOR_LINE
         REGEX "^#define[ \t]+__GNU_MP_VERSION_MINOR[ \t]+[0-9]+")
    file(STRINGS "${GMP_INCLUDE_DIR}/gmp.h" GMP_VERSION_PATCH_LINE
         REGEX "^#define[ \t]+__GNU_MP_VERSION_PATCHLEVEL[ \t]+[0-9]+")

    string(REGEX REPLACE "^#define[ \t]+__GNU_MP_VERSION[ \t]+([0-9]+).*" "\\1"
           GMP_VERSION_MAJOR "${GMP_VERSION_LINE}")
    string(REGEX REPLACE "^#define[ \t]+__GNU_MP_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
           GMP_VERSION_MINOR "${GMP_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+__GNU_MP_VERSION_PATCHLEVEL[ \t]+([0-9]+).*" "\\1"
           GMP_VERSION_PATCH "${GMP_VERSION_PATCH_LINE}")

    set(GMP_VERSION "${GMP_VERSION_MAJOR}.${GMP_VERSION_MINOR}.${GMP_VERSION_PATCH}")
endif()

# Handle standard arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP
    REQUIRED_VARS
        GMP_LIBRARY
        GMP_INCLUDE_DIR
    VERSION_VAR GMP_VERSION
)

if(GMP_FOUND)
    set(GMP_LIBRARIES ${GMP_LIBRARY})
    set(GMP_INCLUDE_DIRS ${GMP_INCLUDE_DIR})

    # Create imported target
    if(NOT TARGET GMP::GMP)
        add_library(GMP::GMP UNKNOWN IMPORTED)
        set_target_properties(GMP::GMP PROPERTIES
            IMPORTED_LOCATION "${GMP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}"
        )
    endif()
endif()

# Mark variables as advanced
mark_as_advanced(
    GMP_INCLUDE_DIR
    GMP_LIBRARY
)
