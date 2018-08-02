CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
project(elfgames_tutorial)

# Source files

set(ELFGAMES_TUTORIAL_SOURCES
    dummy.cc
)

# Main Tutorial library

add_library(elfgames_tutorial ${ELFGAMES_TUTORIAL_SOURCES})
target_link_libraries(elfgames_tutorial PUBLIC
    elf
)

# Python bindings

pybind11_add_module(_elfgames_tutorial pybind_module.cc)
target_link_libraries(_elfgames_tutorial PRIVATE
    elfgames_tutorial
)
