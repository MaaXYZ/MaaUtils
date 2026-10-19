cmake_minimum_required(VERSION 3.20)
project(MaaUtilsPipeTests LANGUAGES CXX)

enable_testing()
if(NOT UNIX)
    message(FATAL_ERROR "These regression tests exercise the POSIX pipe reader")
endif()

get_filename_component(MAAUTILS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED CONFIG)

set(MAAUTILS_LIBRARY "" CACHE FILEPATH "Built MaaUtils library for integration testing (omit for standalone tests)")
add_executable(child_pipe_test child_pipe_test.cpp)
target_include_directories(child_pipe_test PRIVATE ${MAAUTILS_SOURCE_DIR}/include)
if(MAAUTILS_LIBRARY)
    # Use the same Boost headers as the library build to preserve its C++ ABI.
    target_link_libraries(child_pipe_test PRIVATE ${MAAUTILS_LIBRARY})
else()
    # Only logging is replaced; pipes, child processes, clocks and cleanup are real.
    target_sources(child_pipe_test PRIVATE
        ${MAAUTILS_SOURCE_DIR}/source/IOStream/ChildPipeIOStream.cpp
        ${MAAUTILS_SOURCE_DIR}/source/IOStream/IOStream.cpp)
    target_include_directories(child_pipe_test BEFORE PRIVATE support)
endif()
target_include_directories(child_pipe_test SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_compile_features(child_pipe_test PRIVATE cxx_std_20)
target_compile_definitions(child_pipe_test PRIVATE BOOST_PROCESS_VERSION=1)
target_link_libraries(child_pipe_test PRIVATE Threads::Threads)
target_compile_options(child_pipe_test PRIVATE -Wall -Wextra -Werror -Wpedantic)

foreach(case silent partial delimiter_timeout binary count delimiter long_delimiter resume continuous eof nonzero inherited_stdout interrupted echo empty)
    add_test(NAME pipe_${case} COMMAND child_pipe_test ${case})
    set_tests_properties(pipe_${case} PROPERTIES TIMEOUT 8)
endforeach()
