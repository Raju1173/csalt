cmake_minimum_required(VERSION 3.16)
project(csalt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(projectConfigs INTERFACE)

target_compile_options(projectConfigs INTERFACE -Wall -Wextra -Wpedantic -Wimplicit-fallthrough -Wshadow -Wnon-virtual-dtor -Wconversion -Woverloaded-virtual -stdlib=libc++)

target_link_options(projectConfigs INTERFACE -stdlib=libc++)

set(HEADER ${CMAKE_CURRENT_SOURCE_DIR}/header)

target_include_directories(projectConfigs INTERFACE
    ${HEADER}
    ${HEADER}/Frontend
    ${HEADER}/IR
    ${HEADER}/Optimizations
    ${HEADER}/Optimizations/TAC
    ${HEADER}/Optimizations/MIR
    ${HEADER}/Backend
)

add_library(FRONTEND STATIC src/Frontend/lexer.cpp src/Frontend/parser.cpp)
target_link_libraries(FRONTEND PUBLIC projectConfigs)

add_library(IR STATIC src/IR/CFGBuilder.cpp src/IR/SSAConstructor.cpp src/IR/TACGenerator.cpp src/IR/TACAnalyses.cpp src/IR/TACEditor.cpp)
target_link_libraries(IR PUBLIC FRONTEND projectConfigs)

set(TAC src/Optimizations/TAC)
set(MIR src/Optimizations/MIR)

add_library(OPTIMIZATIONS STATIC
    ${TAC}/ConstantFolding.cpp
    ${TAC}/AlgebraicSimplification.cpp
    ${TAC}/BranchSimplification.cpp
    ${TAC}/ControlFlowSimplification.cpp
    ${TAC}/DCE.cpp
    ${TAC}/GVN.cpp
    ${TAC}/LICM.cpp
    ${TAC}/CTFE.cpp
)
target_link_libraries(OPTIMIZATIONS PUBLIC IR projectConfigs)

add_executable(csalt src/csalt.cpp)
target_link_libraries(csalt PRIVATE OPTIMIZATIONS)

