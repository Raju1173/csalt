#pragma once

#include <unordered_map>

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
    MIR_OPT,

    REG_ALLOC,
    FPO,
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
