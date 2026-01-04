cmake_minimum_required(VERSION 3.10)

# define the project (CXX is needed for Sylvan)
project(libmedusa LANGUAGES C CXX)

# enable position independent code for shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# sources
set(libsrc
    src/error.c
    src/gates_symb.c
    src/gates.c
    src/hash.c
    src/htab.c
    src/medusa.c
    src/mtbdd_out.c
    src/mtbdd_symb_map.c
    src/mtbdd_symb_val.c
    src/mtbdd.c
    src/qparam.c
    src/sim.c
    src/symb_utils.c
    src/symexp_list.c
    src/symexp.c)

# headers
set(headers
    src/medusa.h)

# create the target library
add_library(medusa SHARED ${libsrc} ${headers})

# include directories
target_include_directories(medusa PRIVATE src)

# --- Dependencies ---

# dependencies - libgmp
find_package(GMP REQUIRED)
target_link_libraries(medusa PRIVATE GMP::GMP)

# dependencies - libsylvan
find_package(Sylvan)
if(SYLVAN_FOUND)
    target_link_libraries(medusa PRIVATE Sylvan::Sylvan)
else()
    # try to fetch Sylvan if not found
    message(STATUS "Sylvan not found, fetching Sylvan...")

    # Sylvan is not buildable with gcc15+ due to a warning treated as error
    # regarding transposed arguments in calloc calls. Disable that specific warning if using gcc.
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-Wno-error=calloc-transposed-args" HAS_CALLOC_FIX)

    if(HAS_CALLOC_FIX)
        # this prevents the specific transposed-args warning from being treated as an error
        add_compile_options("-Wno-error=calloc-transposed-args")
    endif()

    include(FetchContent)

    FetchContent_Declare(
        sylvan
        GIT_REPOSITORY https://github.com/trolando/sylvan.git
        GIT_TAG        v1.8.0
    )

    FetchContent_MakeAvailable(sylvan)
    target_link_libraries(medusa PRIVATE sylvan)
endif()

# install the .so into the python package folder
install(TARGETS medusa
    COMPONENT runtime
    LIBRARY DESTINATION qiskit_medusa
)
