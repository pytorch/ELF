CMAKE_MINIMUM_REQUIRED(VERSION 3.3)
project(elfgames_tasks)

# Source files

set(ELFGAMES_TASKS_SOURCES
    base/board_feature.cc
#    base/common.cc
#    base/go_state.cc
#    base/board.cc
#    sgf/sgf.cc
    common/game_selfplay.cc
    common/go_state_ext.cc
    train/client_manager.cc
    train/game_train.cc
    train/Pybind.cc
)

set(ELFGAMES_TASKS_INFERENCE_SOURCES
    base/board_feature.cc
#    base/common.cc
#    base/go_state.cc
#    base/board.cc
#    sgf/sgf.cc
    common/game_selfplay.cc
    common/go_state_ext.cc
    inference/Pybind.cc
)

# Main Go library

add_library(elfgames_tasks ${ELFGAMES_TASKS_SOURCES})
if(${BOARD9x9})
    message("Use 9x9 board")
    target_compile_definitions(elfgames_tasks PUBLIC BOARD9x9)
endif()
target_link_libraries(elfgames_tasks PUBLIC
    cppzmq
    elf
)

add_library(elfgames_tasks_inference ${ELFGAMES_TASKS_INFERENCE_SOURCES})
if(${BOARD9x9})
    message("Use 9x9 board")
    target_compile_definitions(elfgames_tasks_inference PUBLIC BOARD9x9)
endif()
target_link_libraries(elfgames_tasks_inference PUBLIC
    elf
)

# For unit-test purpose, build 9x9 library
add_library(elfgames_tasks9 ${ELFGAMES_TASKS_SOURCES})
target_compile_definitions(elfgames_tasks9 PUBLIC BOARD9x9)
target_link_libraries(elfgames_tasks9 PUBLIC
    cppzmq
    elf
)

# Python bindings

pybind11_add_module(_elfgames_tasks train/pybind_module.cc)
target_link_libraries(_elfgames_tasks PRIVATE
    elfgames_tasks
    zmq
)

pybind11_add_module(_elfgames_tasks_inference inference/pybind_module.cc)
target_link_libraries(_elfgames_tasks_inference PRIVATE
    elfgames_tasks_inference
)

#set_target_properties(_elfgames_tasks PROPERTIES
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# unit-test here:
set(TASKS_TEST_SOURCES
#    base/test/coord_test.cc
#    base/test/go_test.cc
#    base/test/board_feature_test.cc
#    base/test/symmetry_test.cc
#    sgf/sgf_test.cc
#    #mcts/mcts_test.cc
)
enable_testing()
add_cpp_tests(test_cpp_elfgames_tasks_ elfgames_tasks9 ${TASKS_TEST_SOURCES})
