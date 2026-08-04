#include "csalt.h"
#include "MIREditor.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("Error : Exactly one file name expected\n");
        return 1;
    }

    gCompilerOptions[Phase::CTFE].enabled = false;

    for (int i = 1; i < argc - 1; i++)
    {
        if (!ParseCompileFlag(std::string(argv[i])))
            return 1;
    }

    std::ifstream file(argv[argc - 1]);

    if (!file.is_open())
    {
        std::print("Error : Could not open file\n");
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    TokenStream TokenStream = Tokenize(source);
    Degubber::TakeSnapshot("TOKEN STREAM", TokenStream, gCompilerOptions[Phase::TOK]);

    Node AST = Parse(TokenStream);
    Degubber::TakeSnapshot("ABSTRACT SYNTAX TREE", AST, gCompilerOptions[Phase::AST]);

    CFG CFG = ConstructCFG(AST);
    Degubber::TakeSnapshot("CONTROL FLOW GRAPH", CFG, gCompilerOptions[Phase::CFG]);

    TAC TAC = GenerateTAC(CFG);
    Degubber::TakeSnapshot("TAC AFTER CONSTRUCTION", TAC, gCompilerOptions[Phase::TAC_CONST]);

    InsertPhiNodes(TAC);
    Degubber::TakeSnapshot("TAC AFTER PHI INSERTION", TAC, gCompilerOptions[Phase::TAC_PHI_INS]);

    RenameVariables(TAC);
    Degubber::TakeSnapshot("TAC AFTER SSA RENAMING", TAC, gCompilerOptions[Phase::TAC_RENAME]);

    // clang-format off
    PassManager<::TAC> TACPassManager(
        {
            PassGroup<::TAC>{
                .IterateToFixedPoint = false,
                .Passes = {
                    // CTFE stays disabled by default an during benchmarks since its basically a cheat code...
                    {Pass<::TAC>{"Compile Time Function Evaluation", gCompilerOptions[Phase::CTFE], {.Run = EvaluateConstantFunctions}}},
                }},

            PassGroup<::TAC>{
                .IterateToFixedPoint = true,
                .Passes = {
                    {Pass<::TAC>{"Constant Folding", gCompilerOptions[Phase::FOLDING], {.RunIter = FoldConstants}}},
                    {Pass<::TAC>{"Algebraic Simplification", gCompilerOptions[Phase::ALG_SIMP], {.RunIter = SimplifyAlgebra}}},
                    {Pass<::TAC>{"Branch Simplification", gCompilerOptions[Phase::BRN_SIMP], {.RunIter = SimplifyBranches}}},
                    {Pass<::TAC>{"Control Flow Simplification", gCompilerOptions[Phase::CFG_SIMP], {.RunIter = SimplifyControlFlow}}},
                    {Pass<::TAC>{"Dead Code Elimination", gCompilerOptions[Phase::DCE], {.RunIter = RemoveDeadCode}}},
                    {Pass<::TAC>{"Global Value Numbering", gCompilerOptions[Phase::GVN], {.RunIter = GVN}}},
                }},

            PassGroup<::TAC>{
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::TAC>{"Loop Invariant Code Motion", gCompilerOptions[Phase::LICM], {.Run = HoistLoopInvariants}}},
                }},
        });
    // clang-format on

    TACPassManager.RunOptimizations(TAC);
    Degubber::TakeSnapshot("TAC AFTER OPTIMIZATIONS", TAC, gCompilerOptions[Phase::TAC_OPT]);

    ResolvePhiNodes(TAC);
    Degubber::TakeSnapshot("TAC AFTER PHI RESOLUTION", TAC, gCompilerOptions[Phase::TAC_PHI_RES]);
    Degubber::TakeSnapshot("TAC", TAC, gCompilerOptions[Phase::TAC]);

    MIR MIR = GenerateMachineIR(TAC);
    Degubber::TakeSnapshot("MIR AFTER CONSTRUCTION", MIR, gCompilerOptions[Phase::MIR_CONST]);

    if (gCompilerOptions[Phase::REG_ALLOC].enabled)
    {
        ResolveVRegsLinearScan(MIR);
        Degubber::TakeSnapshot("MIR AFTER LINEAR SCAN REGISTER ALLOCATION", MIR, gCompilerOptions[Phase::REG_ALLOC]);
    }

    else
    {
        ResolveVRegsSpillAll(MIR);
        Degubber::TakeSnapshot("MIR AFTER SPILL ALLOCATION", MIR, gCompilerOptions[Phase::REG_ALLOC]);
    }

    /*
    PassManager<::MIR> MIRPassManager({PassGroup<::MIR>{
        .IterateToFixedPoint = false,
        .Passes = {
            {Pass<::MIR>{"Frame Pointer Omission", !opts.disableFPO, false, {.Run = OmitFramePointers}}},
        }}});

    MIRPassManager.RunOptimizations(MIR);
    DumpIf(opts.dumpMIR || opts.dumpMIROpt, MIR);
    */

    std::string AssemblyFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 1) + "s";

    EmitAssembly(MIR, AssemblyFilePath);
    Degubber::TakeSnapshot("ASSEMBLY", AssemblyFilePath, gCompilerOptions[Phase::ASM]);

    std::string ExecutableFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 2);

    EmitExecutable(AssemblyFilePath, ExecutableFilePath);

    Degubber::Run();

    PrintOutput(ExecutableFilePath);

    return 0;
}
