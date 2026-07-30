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

    MIR_CONST,
    MIR_OPT,

    FOLDING,
    ALG_SIMP,
    BRN_SIMP,
    DCE,
    CFG_SIMP,
    GVN,
    FPO,
    SCO,
    CTFE,
    LICM
};

enum class DumpMode
{
    STATIC,
    STATIC_HISTORY,
    INTERACTIVE,
    INTERACTIVE_HISTORY,

    NONE,
};

struct SnapshotOptions
{
    bool enabled = true; // for optimization passes...
    DumpMode dumpMode = DumpMode::NONE;
};

using CompilerOptions = std::unordered_map<Phase, SnapshotOptions>;

inline CompilerOptions gCompilerOptions;
