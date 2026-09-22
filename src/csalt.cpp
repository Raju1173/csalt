#include "csalt.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("Error : Exactly one file name expected\n");
        return 1;
    }

    // CTFE stays disabled by default and during benchmarks since its basically a cheat code given this compiler's constraints...
    gCompilerOptions[Phase::CTFE].enabled = false;

    for (int i = 1; i < argc - 1; i++)
    {
        if (!ParseCompileFlag(std::string(argv[i])))
        {
            std::print(stderr, "Error : Invalid flag '{}'\n", std::string(argv[i]));
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

    TokenStream TokenStream;
    Tokenize(source, TokenStream);

    Node AST;
    Parse(TokenStream, AST);

    CFG CFG;
    ConstructCFG(AST, CFG);

    TAC TAC;
    GenerateTAC(CFG, TAC);

    // clang-format off
    PassManager<::TAC> TACPassManagerPreSSA(
        {
            PassGroup<::TAC>
            {
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::TAC>{Phase::TRE, {.Run = EliminateTailRecursions}}},
                    {Pass<::TAC>{Phase::LOOP_INV, {.Run = InvertLoops}}},
                    {Pass<::TAC>{Phase::INLINING, {.Run = InlineFunctions}}},
                }
            },
        });
    // clang-format on

    TACPassManagerPreSSA.RunOptimizations(TAC, Phase::TAC_PRE_SSA_OPT);

    InsertPhiNodes(TAC);

    RenameVariables(TAC);

    // clang-format off
    PassManager<::TAC> TACPassManagerPostSSA(
        {
            PassGroup<::TAC>
            {
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::TAC>{Phase::CTFE, {.Run = EvaluateConstantFunctions}}},
                }
            },

            PassGroup<::TAC>
            {
                .IterateToFixedPoint = true,
                .Passes = {
                    {Pass<::TAC>{Phase::FOLDING, {.RunIter = FoldConstants}}},
                    {Pass<::TAC>{Phase::ALG_SIMP, {.RunIter = SimplifyAlgebra}}},
                    {Pass<::TAC>{Phase::BRN_SIMP, {.RunIter = SimplifyBranches}}},
                    {Pass<::TAC>{Phase::CFG_SIMP, {.RunIter = SimplifyControlFlow}}},
                    {Pass<::TAC>{Phase::DCE, {.RunIter = RemoveDeadCode}}},
                    {Pass<::TAC>{Phase::GVN, {.RunIter = GVN}}},
                    {Pass<::TAC>{Phase::IVM, {.RunIter = MergeInductionVariables}}},
                    {Pass<::TAC>{Phase::LSR, {.RunIter = ReduceLoopMultiplications}}},
                    {Pass<::TAC>{Phase::PHI_SIMP, {.RunIter = SimplifyPhiNodes}}},
                }
            },

            PassGroup<::TAC>
            {
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::TAC>{Phase::LICM, {.Run = HoistLoopInvariants}}},
                    {Pass<::TAC>{Phase::UNROLLING, {.Run = UnrollLoops}}},
                    {Pass<::TAC>{Phase::SCO, {.Run = MarkSiblingCalls}}},
                    {Pass<::TAC>{Phase::IF_CONV, {.Run = IfConversion}}},
                }
            },
        });
    // clang-format on

    TACPassManagerPostSSA.RunOptimizations(TAC, Phase::TAC_POST_SSA_OPT);

    SplitCriticalEdges(TAC);

    ResolvePhiNodes(TAC);

    Debugger::Notify(Phase::TAC);

    MIR MIR;
    GenerateMachineIR(TAC, MIR);

    if (gCompilerOptions[Phase::MIR_REG_ALLOC].enabled)
        ResolveVRegsLinearScan(MIR);
    else
        ResolveVRegsSpillAll(MIR);

    // clang-format off
    PassManager<::MIR> MIRPassManager(
        {
            PassGroup<::MIR>
            {
                .IterateToFixedPoint = false,
                .Passes = {
                    {Pass<::MIR>{Phase::FPO, {.Run = OmitFramePointers}}},
                }
            },
        });
    // clang-format on

    MIRPassManager.RunOptimizations(MIR, Phase::MIR_OPT);

    Debugger::Notify(Phase::MIR);

    std::string AssemblyFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 1) + "s";
    EmitAssembly(MIR, AssemblyFilePath);

    std::string ExecutableFilePath = std::string(argv[argc - 1], 0, std::strlen(argv[argc - 1]) - 1) + "out";
    EmitExecutable(AssemblyFilePath, ExecutableFilePath);

    PrintOutput(ExecutableFilePath);

    return 0;
}
