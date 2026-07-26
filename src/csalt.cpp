#include "csalt.h"
#include "TACGenerator.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("Error : Exactly one file name expected\n");
        return 1;
    }

    for (int i = 1; i < argc - 1; i++)
    {
        std::string arg(argv[i]);

        if (auto opt = argHandlers.find(arg); opt != argHandlers.end())
        {
            opt->second();
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
    DumpIf(opts.dumpTOK, TokenStream);

    Node AST = Parse(TokenStream);
    DumpIf(opts.dumpAST, AST);

    CFG CFG = ConstructCFG(AST);
    DumpIf(opts.dumpCFG, CFG);

    TAC TAC = GenerateTAC(CFG);
    DumpIf(opts.dumpTACConst, TAC);

    InsertPhiNodes(TAC);
    DumpIf(opts.dumpTACPhiIns, TAC);

    RenameVariables(TAC);
    DumpIf(opts.dumpTACRename, TAC);

    // clang-format off
    PassManager<::TAC> TACPassManager(
        {
            PassGroup<::TAC>{
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::TAC>{"Compile Time Function Evaluation", !opts.disableCTFE, opts.historyCTFE, {.Run = EvaluateConstantFunctions}}},
                }},

            PassGroup<::TAC>{
                .IterateToFixedPoint = true,
                .Passes = {
                    {Pass<::TAC>{"Constant Folding", !opts.disableFolding, opts.historyFolding, {.RunIter = FoldConstants}}},
                    {Pass<::TAC>{"Algebraic Simplification", !opts.disableAlgSimp, opts.historyAlgSimp, {.RunIter = SimplifyAlgebra}}},
                    {Pass<::TAC>{"Branch Simplification", !opts.disableBrnSimp, opts.historyBrnSimp, {.RunIter = SimplifyBranches}}},
                    {Pass<::TAC>{"Control Flow Simplification", !opts.disableCFGSimp, opts.historyCFGSimp, {.RunIter = SimplifyControlFlow}}},
                    {Pass<::TAC>{"Dead Code Elimination", !opts.disableDCE, opts.historyDCE, {.RunIter = RemoveDeadCode}}},
                    {Pass<::TAC>{"Global Value Numbering", !opts.disableGVN, opts.historyGVN, {.RunIter = GVN}}},
                }},

            PassGroup<::TAC>{
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::TAC>{"Loop Invariant Code Motion", !opts.disableLICM, opts.historyLICM, {.Run = HoistLoopInvariants}}},
                }},
        });
    // clang-format on

    TACPassManager.RunOptimizations(TAC);
    DumpIf(opts.dumpTACOpt, TAC);

    if (opts.historyTAC)
        PrintTAC(TAC, true);

    ResolvePhiNodes(TAC);
    DumpIf(opts.dumpTAC || opts.dumpTACPhiRes, TAC);
    /*
    MIR MIR = GenerateMachineIR(TAC);
    DumpIf(opts.dumpMIRConst, MIR);

    PassManager<::MIR> MIRPassManager({PassGroup<::MIR>{
        .IterateToFixedPoint = false,
        .Passes = {
            {Pass<::MIR>{"Frame Pointer Omission", !opts.disableFPO, false, {.Run = OmitFramePointers}}},
        }}});

    MIRPassManager.RunOptimizations(MIR);
    DumpIf(opts.dumpMIR || opts.dumpMIROpt, MIR);

    std::string AssemblyFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 1) + "s";

    EmitAssembly(MIR, AssemblyFilePath);
    DumpIf(opts.dumpASM, AssemblyFilePath);

    std::string ExecutableFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 2);

    EmitExecutable(AssemblyFilePath, ExecutableFilePath);

    PrintOutput(ExecutableFilePath);
*/
    return 0;
}
