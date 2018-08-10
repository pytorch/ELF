.PHONY: all
all: elf elfgames/go elfgames/tutorial 

.PHONY: clean
clean:
	rm -rf build/

.PHONY: test
test: test_cpp

.PHONY: test_cpp
test_cpp: test_cpp_elf test_cpp_elfgames_go

build/Makefile: CMakeLists.txt */CMakeLists.txt
	mkdir -p build
	#(cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_FLAGS=-fsanitize=address ..)
	#(cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-fsanitize=address ..)
	#(cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug ..)
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

.PHONY: elfgames/tutorial
elfgames/tutorial: build/Makefile
	(cd build && cmake --build elfgames/tutorial -- -j)

.PHONY: elfgames/tutorial_distri
elfgames/tutorial_distri: build/Makefile
	(cd build && cmake --build elfgames/tutorial_distri -- -j)
