cmake_minimum_required(VERSION 3.3)
project(elf)

# Source files

set(ELF_SOURCES
    Pybind.cc
    concurrency/Counter.cc
    logging/IndexedLoggerFactory.cc
    logging/Levels.cc
    logging/Pybind.cc
    options/OptionMap.cc
    options/OptionSpec.cc
    options/Pybind.cc
)

# set(ELF_TEST_SOURCES
#     options/OptionMapTest.cc
#     options/OptionSpecTest.cc
# )

# Main ELF library

add_library(elf ${ELF_SOURCES})
target_compile_definitions(elf PUBLIC
    GIT_COMMIT_HASH=${GIT_COMMIT_HASH}
    GIT_STAGED=${GIT_STAGED_STRING}
)

target_link_libraries(elf PUBLIC
    #${Boost_LIBRARIES}
    concurrentqueue
    nlohmann_json
    pybind11
    $<BUILD_INTERFACE:${PYTHON_LIBRARIES}>
    spdlog
    ${TBB_IMPORTED_TARGETS}
)

# Tests

enable_testing()
# add_cpp_tests(test_cpp_elf_ elf ${ELF_TEST_SOURCES})

# Python bindings

pybind11_add_module(_elf pybind_module.cc)
target_link_libraries(_elf PRIVATE
    elf
)
#set_target_properties(_elf PROPERTIES
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
