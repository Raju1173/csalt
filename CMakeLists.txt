cmake_minimum_required(VERSION 3.16)
project(epoll_http_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(projectConfigs INTERFACE)

target_compile_options(projectConfigs INTERFACE -Wall -Wextra -Wpedantic -stdlib=libc++)

target_link_options(projectConfigs INTERFACE -stdlib=libc++)

target_include_directories(projectConfigs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/header)

add_executable(csalt src/csalt.cpp src/lexer.cpp src/parser.cpp src/CFGBuilder.cpp src/SSAConstructor.cpp src/TACGenerator.cpp)
target_link_libraries(csalt PRIVATE projectConfigs)
