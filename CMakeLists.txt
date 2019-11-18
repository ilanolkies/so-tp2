cmake_minimum_required(VERSION 3.12.3)  # CMake version check
project(TP1-Sistemas-Operativos)      # Create project "simple_example"
set(CMAKE_CXX_STANDARD 14)            # Enable c++14 standard
SET(CMAKE_CXX_FLAGS -pthread)

# Add files of project root directory as source file
set(SOURCE_FILES block.h block.cpp node.h node.cpp picosha2.h blockchain.cpp)

# Add executable target with source files listed in SOURCE_FILES variable
add_executable(mpiexec ${SOURCE_FILES})
