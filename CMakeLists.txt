cmake_minimum_required(VERSION 3.10)
project(game-ai-lab CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(lab
    src/main.cpp
)

target_include_directories(lab PRIVATE include)

enable_testing()
add_test(NAME lab_unit_tests COMMAND lab test)
