cmake_minimum_required(VERSION 3.16)
project(csalt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(projectConfigs INTERFACE)

target_compile_options(projectConfigs INTERFACE -Wall -Wextra -Wpedantic -Wimplicit-fallthrough -Wshadow -Wnon-virtual-dtor -Wconversion -Woverloaded-virtual -stdlib=libc+ -stdlib=libc++)

target_link_options(projectConfigs INTERFACE -stdlib=libc++)

set(HEADER ${CMAKE_CURRENT_SOURCE_DIR}/header)

target_include_directories(projectConfigs INTERFACE ${HEADER}/Frontend ${HEADER}/IR ${HEADER}/Optimizations ${HEADER}/Optimizations/TAC ${HEADER}/Optimizations/MIR ${HEADER}/Backend)

set(FRONTEND src/Frontend/lexer.cpp src/Frontend/parser.cpp)

set(IR src/IR/CFGBuilder.cpp src/IR/SSAConstructor.cpp src/IR/TACGenerator.cpp src/IR/MIRGenerator.cpp)

set(TAC src/Optimizations/TAC)
set(MIR src/Optimizations/MIR)

set(OPTIMIZATIONS src/Optimizations/PassManager.cpp ${TAC}/ConstantFolding.cpp ${TAC}/BranchSimplification.cpp ${TAC}/AlgebraicSimplification.cpp ${TAC}/DCE.cpp ${TAC}/ControlFlowSimplification.cpp ${TAC}/GVN.cpp ${MIR}/FramePointerOmission.cpp)

set(BACKEND src/Backend/ASMGenerator.cpp)

add_executable(csalt src/csalt.cpp ${FRONTEND} ${IR} ${OPTIMIZATIONS} ${BACKEND})

target_link_libraries(csalt PRIVATE projectConfigs)
