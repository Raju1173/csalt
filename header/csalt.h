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
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <cstring>
#include <fstream>

struct CompilerOptions
{
    bool dumpTOK = false;
    bool dumpAST = false;
    bool dumpCFG = false;
    bool dumpTAC = false;
    bool dumpMIR = false;
    bool dumpASM = false;

    bool dumpTACConst = false;
    bool dumpTACPhiIns = false;
    bool dumpTACRename = false;
    bool dumpTACOpt = false;
    bool dumpTACPhiRes = false;

    bool dumpMIRConst = false;
    bool dumpMIROpt = false;

    bool disableFolding = false;
    bool disableAlgSimp = false;
    bool disableBrnSimp = false;
    bool disableDCE = false;
    bool disableCFGSimp = false;
    bool disableGVN = false;
    bool disableFPO = false;
    bool disableSCO = false;
    bool disableCTFE = true;
    bool disableLICM = false;

    bool historyFolding = false;
    bool historyAlgSimp = false;
    bool historyBrnSimp = false;
    bool historyDCE = false;
    bool historyCFGSimp = false;
    bool historyGVN = false;
    bool historyCTFE = false;
    bool historyLICM = false;
    bool historyTAC = false;

    bool historyFPO = false;
    bool historySCO = false;
    bool historyMIR = false;
};

inline void PrintIR(TokenStream& tokenStream) { PrintTokens(tokenStream); }
inline void PrintIR(Node& AST) { PrintAST(AST); }
inline void PrintIR(CFG& CFG) { PrintCFG(CFG); }
inline void PrintIR(TAC& TAC) { PrintTAC(TAC); }
inline void PrintIR(MIR& MIR) { PrintMIR(MIR); }
inline void PrintIR(std::string AsmFilePath) { PrintASM(AsmFilePath); }

template<typename T> void DumpIf(bool condition, T& obj)
{
    if (condition)
    {
        PrintIR(obj);
    }
}

static CompilerOptions opts;

static std::unordered_map<std::string_view, std::function<void()>> argHandlers = {
    {"dump-tok", []() { opts.dumpTOK = true; }},
    {"dump-ast", []() { opts.dumpAST = true; }},
    {"dump-cfg", []() { opts.dumpCFG = true; }},
    {"dump-tac", []() { opts.dumpTAC = true; }},
    {"dump-mir", []() { opts.dumpMIR = true; }},
    {"dump-asm", []() { opts.dumpASM = true; }},
    {"dump-all", []() { opts.dumpTOK = opts.dumpAST = opts.dumpCFG = opts.dumpTAC = opts.dumpASM = true; }},

    {"dump-tac-const", []() { opts.dumpTACConst = true; }},
    {"dump-tac-phi-ins", []() { opts.dumpTACPhiIns = true; }},
    {"dump-tac-rename", []() { opts.dumpTACRename = true; }},
    {"dump-tac-opt", []() { opts.dumpTACOpt = true; }},
    {"dump-tac-phires", []() { opts.dumpTACPhiRes = true; }},

    {"dump-mir-const", []() { opts.dumpMIRConst = true; }},
    {"dump-mir-opt", []() { opts.dumpMIROpt = true; }},

    {"disable-folding", []() { opts.disableFolding = true; }},
    {"disable-algsimp", []() { opts.disableAlgSimp = true; }},
    {"disable-brnsimp", []() { opts.disableBrnSimp = true; }},
    {"disable-dce", []() { opts.disableDCE = true; }},
    {"disable-cfgsimp", []() { opts.disableCFGSimp = true; }},
    {"disable-gvn", []() { opts.disableGVN = true; }},
    {"disable-fpo", []() { opts.disableFPO = true; }},
    {"disable-sco", []() { opts.disableSCO = true; }},
    {"disable-ctfe", []() { opts.disableCTFE = true; }},
    {"disable-licm", []() { opts.disableLICM = true; }},
    {"disable-all", []() {
         opts.disableFolding = opts.disableAlgSimp = opts.disableBrnSimp = opts.disableDCE = opts.disableCFGSimp = opts.disableGVN = opts.disableFPO = opts.disableSCO = opts.disableCTFE = opts.disableLICM = true;
     }},

    {"enable-folding", []() { opts.disableFolding = false; }},
    {"enable-algsimp", []() { opts.disableAlgSimp = false; }},
    {"enable-brnsimp", []() { opts.disableBrnSimp = false; }},
    {"enable-dce", []() { opts.disableDCE = false; }},
    {"enable-cfgsimp", []() { opts.disableCFGSimp = false; }},
    {"enable-gvn", []() { opts.disableGVN = false; }},
    {"enable-fpo", []() { opts.disableFPO = false; }},
    {"enable-sco", []() { opts.disableSCO = false; }},
    {"enable-ctfe", []() { opts.disableCTFE = false; }},
    {"enable-licm", []() { opts.disableLICM = false; }},

    {"history-folding", []() { opts.historyFolding = true; }},
    {"history-algsimp", []() { opts.historyAlgSimp = true; }},
    {"history-brnsimp", []() { opts.historyBrnSimp = true; }},
    {"history-dce", []() { opts.historyDCE = true; }},
    {"history-cfgsimp", []() { opts.historyCFGSimp = true; }},
    {"history-gvn", []() { opts.historyGVN = true; }},
    {"history-ctfe", []() { opts.historyCTFE = true; }},
    {"history-licm", []() { opts.historyLICM = true; }},
    {"history-tac", []() { opts.historyTAC = true; }},

    {"history-fpo", []() { opts.historyFPO = true; }},
    {"history-sco", []() { opts.historySCO = true; }},
    {"history-mir", []() { opts.historyMIR = true; }},
};
