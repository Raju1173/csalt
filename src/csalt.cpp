#include "ASMGenerator.h"
#include "CFGBuilder.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "ConstantFolding.h"
#include "AlgebraicSimplification.h"
#include "BranchSimplification.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("Error : Exactly one file name expected\n");

        return 1;
    }

    bool dumpTokens = false;
    bool dumpAST = false;
    bool dumpCFG = false;
    bool dumpTAC = false;
    bool dumpASM = false;

    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == "dump-tok")
        {
            dumpTokens = true;
        }

        else if (std::string(argv[i]) == "dump-ast")
        {
            dumpAST = true;
        }

        else if (std::string(argv[i]) == "dump-cfg")
        {
            dumpCFG = true;
        }

        else if (std::string(argv[i]) == "dump-tac")
        {
            dumpTAC = true;
        }

        else if (std::string(argv[i]) == "dump-asm")
        {
            dumpASM = true;
        }

        else if (std::string(argv[i]) == "dump-all")
        {
            dumpTokens = true;
            dumpAST = true;
            dumpCFG = true;
            dumpTAC = true;
            dumpASM = true;
        }

        else
        {
            std::print("Error : Invalid argument\n");
            return 1;
        }
    }

    std::ifstream file(argv[argc - 1]);

    if (!file.is_open())
    {
        std::print("Error : Could not open file\n");
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<Token> TokenStream = tokenize(source);

    if (dumpTokens)
    {
        std::print("------TOKENS-------\n\n");

        printTokens(TokenStream);
    }

    Node AST = parse(TokenStream);

    if (dumpAST)
    {
        std::print("\n------AST-------\n\n");

        printNode(AST);

        std::print("\n");
    }

    std::vector<std::unique_ptr<CFGFunction>> CFG = constructCFG(AST);

    ComputeDominators(CFG);

    ComputeDominatorTree(CFG);

    for (auto& CFGFunc : CFG)
    {
        ComputeFrontiers(CFGFunc->Blocks[0].get());
    }

    InsertPhiNodes(CFG);

    RenameVariables(CFG);

    if (dumpCFG)
    {
        std::print("\n------CFG-------\n");

        printCFG(CFG);
    }

    auto TAC = GenerateTAC(CFG);

    ResolvePhiNodes(TAC);

    FoldConstants(TAC);

    SimplifyAlgebra(TAC);

    SimplifyBranches(TAC);

    if (dumpTAC)
    {
        std::print("\n------TAC-------\n\n");

        printTAC(TAC);
    }

    auto MIR = GenerateMachineIR(TAC);

    std::string AssemblyFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 1) + "s";

    EmitAssembly(MIR, AssemblyFilePath);

    if (dumpASM)
    {
        std::print("\n------ASSEMBLY-----\n\n");

        std::ifstream AsmFile(AssemblyFilePath, std::ios::in | std::ios::binary | std::ios::ate);

        std::streamsize size = AsmFile.tellg();

        AsmFile.seekg(0, std::ios::beg);

        std::string AsmOutput(size, '\0');

        AsmFile.read(AsmOutput.data(), size);

        std::print("{}", AsmOutput);
    }

    std::string ExecutableFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 2);

    EmitExecutable(AssemblyFilePath, ExecutableFilePath);

    PrintOutput(ExecutableFilePath);

    return 0;
}
