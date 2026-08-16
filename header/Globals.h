#pragma once

#include <string>
#include <unordered_map>
#include <utility>

enum class Phase
{
    TOK,
    AST,
    CFG,
    TAC,
    MIR,
    ASM,

    TAC_CONST,
    TAC_PHI_INS,
    TAC_RENAME,
    TAC_OPT,
    TAC_PHI_RES,

    FOLDING,
    ALG_SIMP,
    BRN_SIMP,
    DCE,
    CFG_SIMP,
    GVN,
    SCO,
    CTFE,
    LICM,

    MIR_CONST,
    MIR_REG_ALLOC,
    MIR_OPT,

    FPO,

    COUNT,
};

struct PhaseOptions
{
    bool enabled = true; // for optimization passes...

    bool Dump = false;
    bool HistoryDump = false;
    bool InteractiveDump = false;
    bool InteractiveHistoryDump = false;
};

using CompilerOptions = std::unordered_map<Phase, PhaseOptions>;

inline CompilerOptions gCompilerOptions;

struct PhaseMetadata
{
    std::string name;

    bool isTOKPhase = false;
    bool isASTPhase = false;
    bool isCFGPhase = false;
    bool isTACPhase = false;
    bool isMIRPhase = false;
    bool isASMPhase = false;

    bool isOptimizationPhase = false;
};

inline PhaseMetadata getPhaseMetadata(Phase pass)
{
    static_assert(std::to_underlying(Phase::COUNT) == 24, "Add another case and increment the count check when adding a new phase!!!");

    switch (pass)
    {
        case Phase::TOK:
            return {.name = "TOKEN STREAM", .isTOKPhase = true};
        case Phase::AST:
            return {.name = "ABSTRACT SYNTAX TREE", .isASTPhase = true};
        case Phase::CFG:
            return {.name = "CONTROL FLOW GRAPH", .isCFGPhase = true};
        case Phase::TAC:
            return {.name = "THREE ADDRESS CODE", .isTACPhase = true};
        case Phase::MIR:
            return {.name = "MACHINE IR", .isMIRPhase = true};
        case Phase::ASM:
            return {.name = "ASSEMBLY", .isASMPhase = true};

        case Phase::TAC_CONST:
            return {.name = "TAC AFTER CONSTRUCTION", .isTACPhase = true};
        case Phase::TAC_PHI_INS:
            return {.name = "TAC AFTER PHI INSERTION", .isTACPhase = true};
        case Phase::TAC_RENAME:
            return {.name = "TAC AFTER SSA RENAMING", .isTACPhase = true};
        case Phase::TAC_OPT:
            return {.name = "TAC AFTER OPTIMIZATIONS", .isTACPhase = true};
        case Phase::TAC_PHI_RES:
            return {.name = "TAC AFTER PHI RESOLUTION", .isTACPhase = true};

        case Phase::FOLDING:
            return {.name = "CONSTANT FOLDING", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::ALG_SIMP:
            return {.name = "ALGEBRAIC SIMPLIFICATION", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::BRN_SIMP:
            return {.name = "BRANCH SIMPLIFICATION", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::DCE:
            return {.name = "DEAD CODE ELIMINATION", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::CFG_SIMP:
            return {.name = "CONTROL FLOW SIMPLIFICATION", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::GVN:
            return {.name = "GLOBAL VALUE NUMBERING", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::SCO:
            return {.name = "SIBLING CALL OPTIMIZATION", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::CTFE:
            return {.name = "COMPILE TIME FUNCTION EXECUTION", .isTACPhase = true, .isOptimizationPhase = true};
        case Phase::LICM:
            return {.name = "LOOP INVARIANT CODE MOTION", .isTACPhase = true, .isOptimizationPhase = true};

        case Phase::MIR_CONST:
            return {.name = "MIR AFTER CONSTRUCTION", .isMIRPhase = true};
        case Phase::MIR_REG_ALLOC:
            if (gCompilerOptions[Phase::MIR_REG_ALLOC].enabled)
                return {.name = "MIR AFTER LINEAR SCAN REGISTER ALLOCATION", .isMIRPhase = true, .isOptimizationPhase = true};
            else
                return {.name = "MIR AFTER SPILL ALLOCATION", .isMIRPhase = true, .isOptimizationPhase = true};
        case Phase::MIR_OPT:
            return {.name = "MIR AFTER OPTIMIZATIONS", .isMIRPhase = true};

        case Phase::FPO:
            return {.name = "FRAME POINTER OMISSION", .isMIRPhase = true, .isOptimizationPhase = true};
    }
}
