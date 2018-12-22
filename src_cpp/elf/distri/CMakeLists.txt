CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
project(elfgames_tutorial_distri)

# Source files

set(ELFGAMES_TUTORIAL_DISTRI_SOURCES
    client_game.cc
    server_game.cc
    Pybind.cc
)

# Main library

add_library(elfgames_tutorial_distri ${ELFGAMES_TUTORIAL_DISTRI_SOURCES})
target_link_libraries(elfgames_tutorial_distri PUBLIC
    cppzmq
    elf
)

# Python bindings
pybind11_add_module(_elfgames_tutorial_distri pybind_module.cc)
target_link_libraries(_elfgames_tutorial_distri PRIVATE
    elfgames_tutorial_distri
    zmq
)
