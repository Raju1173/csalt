#include "MIRGenerator.h"
#include "CFGBuilder.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "ConstantFolding.h"
#include "AlgebraicSimplification.h"
#include "BranchSimplification.h"
#include "ControlFlowSimplification.h"
#include "FramePointerOmission.h"
#include "ASMGenerator.h"
#include "PassManager.h"
#include "GVN.h"
#include "DCE.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <print>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("Error : Exactly one file name expected\n");

        return 1;
    }

    bool dumpTOK = false;
    bool dumpAST = false;
    bool dumpCFG = false;
    bool dumpTAC = false;
    bool dumpMIR = false;
    bool dumpASM = false;

    bool disableFolding = false;
    bool disableAlgSimp = false;
    bool disableBrnSimp = false;
    bool disableDCE = false;
    bool disableCFGSimp = false;
    bool disableGVN = false;

    bool disableFPO = false;

    for (int i = 1; i < argc - 1; i++)
    {
        if (std::string(argv[i]) == "dump-tok")
        {
            dumpTOK = true;
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

        else if (std::string(argv[i]) == "dump-mir")
        {
            dumpMIR = true;
        }

        else if (std::string(argv[i]) == "dump-asm")
        {
            dumpASM = true;
        }

        else if (std::string(argv[i]) == "dump-all")
        {
            dumpTOK = true;
            dumpAST = true;
            dumpCFG = true;
            dumpTAC = true;
            dumpASM = true;
        }

        else if (std::string(argv[i]) == "disable-folding")
        {
            disableFolding = true;
        }

        else if (std::string(argv[i]) == "disable-algsimp")
        {
            disableAlgSimp = true;
        }

        else if (std::string(argv[i]) == "disable-brnsimp")
        {
            disableBrnSimp = true;
        }

        else if (std::string(argv[i]) == "disable-dce")
        {
            disableDCE = true;
        }

        else if (std::string(argv[i]) == "disable-cfgsimp")
        {
            disableCFGSimp = true;
        }

        else if (std::string(argv[i]) == "disable-gvn")
        {
            disableGVN = true;
        }

        else if (std::string(argv[i]) == "disable-fpo")
        {
            disableFPO = true;
        }

        else if (std::string(argv[i]) == "disable-all")
        {
            disableFolding = true;
            disableAlgSimp = true;
            disableBrnSimp = true;
            disableDCE = true;
            disableCFGSimp = true;
            disableGVN = true;

            disableFPO = true;
        }

        else if (std::string(argv[i]) == "enable-folding")
        {
            disableFolding = false;
        }

        else if (std::string(argv[i]) == "enable-algsimp")
        {
            disableAlgSimp = false;
        }

        else if (std::string(argv[i]) == "enable-brnsimp")
        {
            disableBrnSimp = false;
        }

        else if (std::string(argv[i]) == "enable-dce")
        {
            disableDCE = false;
        }

        else if (std::string(argv[i]) == "enable-cfgsimp")
        {
            disableCFGSimp = false;
        }

        else if (std::string(argv[i]) == "enable-gvn")
        {
            disableGVN = false;
        }

        else if (std::string(argv[i]) == "enable-fpo")
        {
            disableFPO = false;
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

    if (dumpTOK)
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

    PassManager TACPassManager({PassGroup{
        .IterateToFixedPoint = true,
        .Passes = {
            {Pass{"Constant Folding", !disableFolding, false, {.RunTACIter = FoldConstants}}},
            {Pass{"Algebraic Simplification", !disableAlgSimp, false, {.RunTACIter = SimplifyAlgebra}}},
            {Pass{"Branch Simplification", !disableBrnSimp, false, {.RunTACIter = SimplifyBranches}}},
            {Pass{"Dead Code Elimination", !disableDCE, false, {.RunTACIter = RemoveDeadCode}}},
            {Pass{"Control Flow Simplification", !disableCFGSimp, false, {.RunTACIter = SimplifyControlFlow}}},
            {Pass{"Global Value Numbering", !disableGVN, false, {.RunTACIter = GVN}}},
        }}});

    TACPassManager.RunOptimizations(&TAC, nullptr);

    if (dumpTAC)
    {
        printTAC(TAC);
    }

    ResolvePhiNodes(TAC);

    if (dumpTAC)
    {
        printTAC(TAC);
    }

    MIR MIR = GenerateMachineIR(TAC);

    PassManager MIRPassManager({PassGroup{
        .IterateToFixedPoint = false,
        .Passes = {
            {Pass{"Frame Pointer Omission", !disableFPO, false, {.RunMIR = OmitFramePointers}}},
        }}});

    if (dumpMIR)
    {
        PrintMIR(MIR);
    }

    std::string AssemblyFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 1) + "s";

    EmitAssembly(MIR, AssemblyFilePath);

    if (dumpASM)
    {
        PrintASM(AssemblyFilePath);
    }

    std::string ExecutableFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 2);

    EmitExecutable(AssemblyFilePath, ExecutableFilePath);

    PrintOutput(ExecutableFilePath);

    return 0;
}
