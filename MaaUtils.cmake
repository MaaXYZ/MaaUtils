set_property(GLOBAL PROPERTY USE_FOLDERS ON)

cmake_policy(SET CMP0155 OLD)

option(BUILD_MAA_UTILS "build maa utils" ON)
option(WITH_RPATH_LIBRARY "with rpath library for linux" ${LINUX})

set(Boost_NO_WARN_NEW_VERSIONS ON)

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON) see below
set(MAAUTILS_DIR ${CMAKE_CURRENT_LIST_DIR})

list(APPEND CMAKE_MODULE_PATH "${MAAUTILS_DIR}/cmake/modules")
set(MAADEPS_DIR ${MAAUTILS_DIR}/MaaDeps)
include(${MAADEPS_DIR}/maadeps.cmake)

# Basic compile and link configuration
include(${MAAUTILS_DIR}/cmake/config.cmake)
include(${MAAUTILS_DIR}/cmake/utils.cmake)
include(${MAAUTILS_DIR}/cmake/version.cmake)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(Boost REQUIRED CONFIG COMPONENTS system)
find_package(ZLIB REQUIRED)
find_package(fastdeploy_ppocr REQUIRED)
find_package(ONNXRuntime REQUIRED)

find_program(CCACHE_PROG ccache)

if(CCACHE_PROG)
    message("Find ccache at ${CCACHE_PROG}")

    if(ENABLE_CCACHE)
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROG})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROG})
    endif()
endif()

include_directories(${MAAUTILS_DIR}/include)

if(BUILD_MAA_UTILS)
    add_subdirectory(${MAAUTILS_DIR}/source ${CMAKE_CURRENT_BINARY_DIR}/MaaUtils)
endif()