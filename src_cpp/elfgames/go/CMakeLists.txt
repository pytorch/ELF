CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
project(elfgames_go)

# Source files

set(ELFGAMES_GO_SOURCES
    Pybind.cc
    base/board_feature.cc
    base/common.cc
    base/go_state.cc
    base/board.cc
    game_train.cc
    sgf/sgf.cc
    game_selfplay.cc
    go_state_ext.cc
    client_manager.cc
)

# Main Go library

add_library(elfgames_go ${ELFGAMES_GO_SOURCES})
if(${BOARD9x9})
    message("Use 9x9 board")
    target_compile_definitions(elfgames_go PUBLIC BOARD9x9)
endif()
target_link_libraries(elfgames_go PUBLIC
    elf
)

# For unit-test purpose, build 9x9 library
add_library(elfgames_go9 ${ELFGAMES_GO_SOURCES})
target_compile_definitions(elfgames_go9 PUBLIC BOARD9x9)
target_link_libraries(elfgames_go9 PUBLIC
    elf
)

# Python bindings

pybind11_add_module(_elfgames_go pybind_module.cc)
target_link_libraries(_elfgames_go PRIVATE
    elfgames_go
    zmq
)
#set_target_properties(_elfgames_go PROPERTIES
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# unit-test here:
set(GO_TEST_SOURCES
    base/coord_test.cc
    base/go_test.cc
    base/board_feature_test.cc
    base/symmetry_test.cc
    sgf/sgf_test.cc
    # mcts/mcts_test.cc
)
enable_testing()
add_cpp_tests(test_cpp_elfgames_go_ elfgames_go9 ${GO_TEST_SOURCES})
