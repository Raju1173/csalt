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

inline std::unordered_map<std::string, Phase> phaseMap = {
    {"tok", Phase::TOK},
    {"ast", Phase::AST},
    {"cfg", Phase::CFG},
    {"tac", Phase::TAC},
    {"mir", Phase::MIR},
    {"asm", Phase::ASM},

    {"tac-const", Phase::TAC_CONST},
    {"tac-phi-ins", Phase::TAC_PHI_INS},
    {"tac-rename", Phase::TAC_RENAME},
    {"tac-opt", Phase::TAC_OPT},
    {"tac-phires", Phase::TAC_PHI_RES},

    {"mir-const", Phase::MIR_CONST},
    {"mir-opt", Phase::MIR_OPT},

    {"folding", Phase::FOLDING},
    {"algsimp", Phase::ALG_SIMP},
    {"brnsimp", Phase::BRN_SIMP},
    {"dce", Phase::DCE},
    {"cfgsimp", Phase::CFG_SIMP},
    {"gvn", Phase::GVN},
    {"fpo", Phase::FPO},
    {"sco", Phase::SCO},
    {"ctfe", Phase::CTFE},
    {"licm", Phase::LICM},
};

inline bool ParseCompileFlag(std::string arg)
{
    std::string target;

    if (arg.starts_with("--interactive-history-dump-"))
    {
        target = arg.substr(27);

        if (phaseMap.contains(target))
            gCompilerOptions[phaseMap.at(target)].dumpMode = DumpMode::INTERACTIVE_HISTORY;
    }

    else if (arg.starts_with("--interactive-dump-"))
    {
        target = arg.substr(19);

        if (phaseMap.contains(target))
            gCompilerOptions[phaseMap.at(target)].dumpMode = DumpMode::INTERACTIVE;
    }

    else if (arg.starts_with("--history-dump-"))
    {
        target = arg.substr(15);

        if (phaseMap.contains(target))
            gCompilerOptions[phaseMap.at(target)].dumpMode = DumpMode::STATIC_HISTORY;
    }

    else if (arg.starts_with("--dump-"))
    {
        target = arg.substr(7);

        if (target == "all")
        {
            for (const auto& [name, phase] : phaseMap)
                gCompilerOptions[phase].dumpMode = DumpMode::STATIC;
            return true;
        }

        if (phaseMap.contains(target))
            gCompilerOptions[phaseMap.at(target)].dumpMode = DumpMode::STATIC;
    }

    else if (arg.starts_with("--enable-"))
    {
        target = arg.substr(9);

        if (phaseMap.contains(target))
            gCompilerOptions[phaseMap.at(target)].enabled = true;
    }

    else if (arg.starts_with("--disable-"))
    {
        target = arg.substr(10);

        if (target == "all")
        {
            for (auto& [name, phase] : phaseMap)
            {
                if (phase >= Phase::FOLDING && phase <= Phase::LICM)
                {
                    gCompilerOptions[phase].enabled = false;
                }
            }
            return true;
        }

        if (phaseMap.contains(target))
            gCompilerOptions[phaseMap.at(target)].enabled = false;
    }

    else
    {
        std::print(stderr, "Error : Invalid flag '{}'\n", arg);
        return false;
    }

    return true;
}
