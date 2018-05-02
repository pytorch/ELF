cmake_minimum_required(VERSION 3.3)

# These projects all have CMakeLists by default, thank you devs!

add_subdirectory(googletest)
add_subdirectory(json)
add_subdirectory(pybind11)
add_subdirectory(spdlog)

# concurrent queue library

add_library(concurrentqueue INTERFACE)
target_include_directories(concurrentqueue SYSTEM INTERFACE
    concurrentqueue
)
target_link_libraries(concurrentqueue INTERFACE
    ${CMAKE_THREAD_LIBS_INIT}
)

# TBB

set(TBB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/tbb/)
set(TBB_ROOT ${TBB_ROOT} PARENT_SCOPE)
include(${TBB_ROOT}/cmake/TBBBuild.cmake)
tbb_build(
    TBB_ROOT ${TBB_ROOT}
    MAKE_ARGS tbb_build_dir=${CMAKE_CURRENT_BINARY_DIR}/tbb/
    CONFIG_DIR TBB_DIR)
set(TBB_DIR ${TBB_DIR} PARENT_SCOPE)

# Find ZMQ in system. TODO(maj): maybe use submodule?

find_path(ZMQ_ROOT_DIR
    NAMES include/zmq.h)
find_library(ZeroMQ_LIBRARY
    NAMES zmq libzmq
    HINTS ${ZMQ_ROOT_DIR}/lib
)
find_path(ZeroMQ_INCLUDE_DIR
    NAMES zmq.h
    HINTS ${ZMQ_ROOT_DIR}/include
)
add_library(zmq INTERFACE)
target_include_directories(zmq SYSTEM INTERFACE ${ZeroMQ_INCLUDE_DIR})
target_link_libraries(zmq INTERFACE ${ZeroMQ_LIBRARY})

# C++ ZMQ

add_library(cppzmq INTERFACE)
target_include_directories(cppzmq SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cppzmq/)
target_link_libraries(cppzmq INTERFACE zmq)
