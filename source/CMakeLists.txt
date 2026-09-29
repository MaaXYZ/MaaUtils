file(GLOB_RECURSE maa_utils_src *.h *.hpp *.cpp)
file(GLOB_RECURSE maa_utils_header ${MAAUTILS_DIR}/include/MaaUtils/*)

add_library(MaaUtils SHARED ${maa_utils_src} ${maa_utils_header})
target_include_directories(MaaUtils PRIVATE ${MAAUTILS_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MaaUtils PUBLIC Boost::regex PRIVATE Boost::system ${OpenCV_LIBS})

if(WIN32)
    target_link_libraries(MaaUtils PRIVATE d3d12 dxgi Cfgmgr32)
endif()

if(LINUX)
    target_link_libraries(MaaUtils PRIVATE pthread dl)
endif()

target_compile_definitions(MaaUtils PRIVATE MAA_UTILS_EXPORTS)

# Boost.Process v1/v2 is a whole-translation-unit choice: BOOST_PROCESS_VERSION is read the first time a
# Boost.Process config header is parsed and cannot be changed afterwards, because those config headers are
# include guarded. Fixing it here keeps every translation unit that links MaaUtils on v1 regardless of the
# order in which Boost.Process headers are included, and keeps both namespaces from ending up inline, which
# would make names shared by v1 and v2 (such as boost::process::environment) ambiguous.
# MaaUtils/IOStream/BoostIO.hpp still scopes the macro around its own includes, as a fallback for consumers
# that only add the include directory instead of linking this target.
# Consequence: inside translation units that link MaaUtils, Boost.Process v2 is no longer an inline
# namespace, so v2 names have to be qualified, e.g. boost::process::v2::pid_type.
target_compile_definitions(MaaUtils PUBLIC BOOST_PROCESS_VERSION=1)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${maa_utils_src})
