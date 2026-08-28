#include "MIRGenerator.h"
#include "CFGBuilder.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "ConstantFolding.h"
#include "AlgebraicSimplification.h"
#include "BranchSimplification.h"
#include "ControlFlowSimplification.h"
#include "FramePointerOmission.h"
#include "SCO.h"
#include "CTFE.h"
#include "LICM.h"
#include "ASMGenerator.h"
#include "PassManager.h"
#include "GVN.h"
#include "DCE.h"
#include "lexer.h"
#include "parser.h"
#include <print>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <fstream>
#include "IRDebugger.h"
#include "Globals.h"
#include "RegisterAllocator.h"
#include "SpillAllocator.h"
#include "LoopInversion.h"

static_assert(std::to_underlying(Phase::COUNT) == 28, "Add another targetPhaseMap entry and increment the count check when adding a new phase!!!");

inline std::unordered_map<std::string, Phase> targetPhaseMap = {
    {"tok", Phase::TOK},
    {"ast", Phase::AST},
    {"cfg", Phase::CFG},
    {"tac", Phase::TAC},
    {"mir", Phase::MIR},
    {"asm", Phase::ASM},

    {"tac-const", Phase::TAC_CONST},
    {"tac-phi-ins", Phase::TAC_PHI_INS},
    {"tac-rename", Phase::TAC_RENAME},
    {"tac-pre-ssa-opt", Phase::TAC_PRE_SSA_OPT},
    {"tac-post-ssa-opt", Phase::TAC_POST_SSA_OPT},
    {"tac-edge-split", Phase::TAC_EDGE_SPLIT},
    {"tac-phi-res", Phase::TAC_PHI_RES},

    {"folding", Phase::FOLDING},
    {"alg-simp", Phase::ALG_SIMP},
    {"brn-simp", Phase::BRN_SIMP},
    {"dce", Phase::DCE},
    {"cfg-simp", Phase::CFG_SIMP},
    {"gvn", Phase::GVN},
    {"sco", Phase::SCO},
    {"ctfe", Phase::CTFE},
    {"loop-inv", Phase::LOOP_INV},
    {"licm", Phase::LICM},

    {"mir-const", Phase::MIR_CONST},
    {"mir-reg-alloc", Phase::MIR_REG_ALLOC},
    {"mir-opt", Phase::MIR_OPT},

    {"fpo", Phase::FPO},

    {"output", Phase::OUTPUT},
};

inline bool ParseCompileFlag(std::string arg)
{
    std::string target;

    if (arg.starts_with("--interactive-history-dump-"))
    {
        target = arg.substr(27);

        if (targetPhaseMap.contains(target))
        {
            gCompilerOptions[targetPhaseMap.at(target)].InteractiveHistoryDump = true;
            return true;
        }
    }

    else if (arg.starts_with("--interactive-dump-"))
    {
        target = arg.substr(19);

        if (targetPhaseMap.contains(target))
        {
            gCompilerOptions[targetPhaseMap.at(target)].InteractiveDump = true;
            return true;
        }
    }

    else if (arg.starts_with("--history-dump-"))
    {
        target = arg.substr(15);

        if (targetPhaseMap.contains(target))
        {
            gCompilerOptions[targetPhaseMap.at(target)].HistoryDump = true;
            return true;
        }
    }

    else if (arg.starts_with("--dump-"))
    {
        target = arg.substr(7);

        if (targetPhaseMap.contains(target))
        {
            gCompilerOptions[targetPhaseMap.at(target)].Dump = true;
            return true;
        }
    }

    else if (arg.starts_with("--enable-"))
    {
        target = arg.substr(9);

        if (targetPhaseMap.contains(target))
        {
            gCompilerOptions[targetPhaseMap.at(target)].enabled = true;
            return true;
        }
    }

    else if (arg.starts_with("--disable-"))
    {
        target = arg.substr(10);

        if (target == "all")
        {
            for (auto& [name, phase] : targetPhaseMap)
            {
                if (getPhaseMetadata(phase).isOptimizationPhase)
                {
                    gCompilerOptions[phase].enabled = false;
                }
            }

            return true;
        }

        if (targetPhaseMap.contains(target))
        {
            gCompilerOptions[targetPhaseMap.at(target)].enabled = false;
            return true;
        }
    }

    else
        return false;

    return false;
}
