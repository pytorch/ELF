.PHONY: all
all: elf elfgames/go

.PHONY: clean
clean:
	rm -rf build/

.PHONY: test
test: test_cpp

.PHONY: test_cpp
test_cpp: test_cpp_elf test_cpp_elfgames_go

build/Makefile: CMakeLists.txt */CMakeLists.txt
	mkdir -p build
	(cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..)

.PHONY: elf
elf: build/Makefile
	(cd build && cmake --build elf -- -j)

.PHONY: test_cpp_elf
test_cpp_elf:
	(cd build/elf && GTEST_COLOR=1 ctest --output-on-failure)

.PHONY: test_cpp_elfgames_go
test_cpp_elfgames_go:
	(cd build/elfgames/go && GTEST_COLOR=1 ctest --output-on-failure)

.PHONY: elfgames/go
elfgames/go: build/Makefile
	(cd build && cmake --build elfgames/go -- -j)
