#include "MIRGenerator.h"
#include "CFGBuilder.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "ConstantFolding.h"
#include "AlgebraicSimplification.h"
#include "BranchSimplification.h"
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
    bool disableGVN = false;

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

        else if (std::string(argv[i]) == "disable-gvn")
        {
            disableGVN = true;
        }

        else if (std::string(argv[i]) == "disable-all")
        {
            disableFolding = true;
            disableAlgSimp = true;
            disableBrnSimp = true;
            disableDCE = true;
            disableGVN = true;
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

        else if (std::string(argv[i]) == "enable-gvn")
        {
            disableGVN = false;
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

    ResolvePhiNodes(TAC);

    //PassManager PM({PassGroup{true, {Pass{"Constant Folding", disableFolding, false, FoldConstants}}}});

    FoldConstants(TAC);

    SimplifyAlgebra(TAC);

    SimplifyBranches(TAC);

    RemoveDeadCode(TAC);

    GVN(TAC);

    if (dumpTAC)
    {
        printTAC(TAC);
    }

    MIR MIR = GenerateMachineIR(TAC);

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
