#include "MIRGenerator.h"
#include "CFGBuilder.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "ConstantFolding.h"
#include "AlgebraicSimplification.h"
#include "BranchSimplification.h"
#include "ASMGenerator.h"
#include "GVN.h"
#include "DCE.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <iostream>
#include <print>
#include <string>

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

    TokenStream TokenStream = Tokenize(source);

    if (dumpTokens)
    {
        PrintTokens(TokenStream);
    }

    Node AST = Parse(TokenStream);

    if (dumpAST)
    {
        PrintAST(AST);
    }

    CFG CFG = ConstructCFG(AST);

    CFG.computeDominators();

    CFG.computeDominatorTree();

    CFG.computeFrontiers();

    InsertPhiNodes(CFG);

    RenameVariables(CFG);

    if (dumpCFG)
    {
        PrintCFG(CFG);
    }

    TAC TAC = GenerateTAC(CFG);

    ResolvePhiNodes(TAC);

    //FoldConstants(TAC);

    //SimplifyAlgebra(TAC);

    //SimplifyBranches(TAC);

    //RemoveDeadCodeAndMergeBlocks(TAC);

    //GVN(TAC);

    if (dumpTAC)
    {
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
