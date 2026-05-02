file(GLOB_RECURSE maa_utils_src *.h *.hpp *.cpp)
file(GLOB_RECURSE maa_utils_header ${MAAUTILS_DIR}/include/MaaUtils/*)

# Embed MAA_HASH_VERSION via configure_file so only this single TU recompiles when version changes.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp
    @ONLY)

add_library(MaaUtils SHARED ${maa_utils_src} ${maa_utils_header} ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp)
target_include_directories(MaaUtils PRIVATE ${MAAUTILS_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MaaUtils PUBLIC Boost::regex PRIVATE Boost::system ${OpenCV_LIBS})

if(WIN32)
    target_link_libraries(MaaUtils PRIVATE d3d12 dxgi Cfgmgr32)
endif()

if(LINUX)
    target_link_libraries(MaaUtils PRIVATE pthread dl)
endif()

target_compile_definitions(MaaUtils PRIVATE MAA_UTILS_EXPORTS)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${maa_utils_src})
