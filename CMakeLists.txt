cmake_minimum_required(VERSION 3.18)

set(PROJECT_NAME task7)
project(${PROJECT_NAME} CXX)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# create executable
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cc src/*.h)

add_executable(${PROJECT_NAME} ${SOURCES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-function)
