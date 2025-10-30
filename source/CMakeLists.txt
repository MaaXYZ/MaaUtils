file(GLOB_RECURSE maa_utils_src *.h *.hpp *.cpp)
file(GLOB_RECURSE maa_utils_header ${MAAUTILS_DIR}/include/Utils/*)

add_library(MaaUtils SHARED ${maa_utils_src} ${maa_utils_header})
target_include_directories(MaaUtils PRIVATE ${MAAUTILS_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MaaUtils PRIVATE Boost::system ${OpenCV_LIBS})

if(WIN32)
    target_link_libraries(MaaUtils PRIVATE d3d12 dxgi Cfgmgr32)
endif()

if(LINUX)
    target_link_libraries(MaaUtils PRIVATE pthread dl)
endif()

target_compile_definitions(MaaUtils PRIVATE MAA_UTILS_EXPORTS)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${maa_utils_src})
