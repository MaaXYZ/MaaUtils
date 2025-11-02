set(CMAKE_CXX_STANDARD 20)

add_executable(StaticEventBusTest StaticEventBusTest.cpp)

target_include_directories(StaticEventBusTest PRIVATE ../include)