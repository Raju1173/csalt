#include "IRDebugger.h"
#include "Globals.h"

void Debugger::AddIR(TokenStream& TokenStream)
{
    if (mTokenStream == nullptr)
        mTokenStream = &TokenStream;
}

void Debugger::AddIR(Node& AST)
{
    if (mAST == nullptr)
        mAST = &AST;
}

void Debugger::AddIR(CFG& CFG)
{
    if (mCFG == nullptr)
        mCFG = &CFG;
}

void Debugger::AddIR(TAC& TAC)
{
    if (mTAC == nullptr)
        mTAC = &TAC;
}

void Debugger::AddIR(MIR& MIR)
{
    if (mMIR == nullptr)
        mMIR = &MIR;
}

void Debugger::AddIR(std::string ASMFilePath)
{
    if (mASMFilePath == "")
        mASMFilePath = ASMFilePath;
}

template<typename R> void Debugger::PrintIR(R* IR, Phase phase)
{
    if (gCompilerOptions[phase].Dump)
    {
        std::print("-----{}-----\n\n", getPhaseMetadata(phase).name);
        std::print("{}", FormatIR(*IR, false, false));
    }

    if (gCompilerOptions[phase].HistoryDump)
    {
        std::print("-----{}-----\n\n", getPhaseMetadata(phase).name);
        std::print("{}", FormatIR(*IR, true, false));
    }

    if (gCompilerOptions[phase].MetaDump)
    {
        std::print("-----{}-----\n\n", getPhaseMetadata(phase).name);
        std::print("{}", FormatIR(*IR, false, true));
    }

    if (gCompilerOptions[phase].HistoryMetaDump)
    {
        std::print("-----{}-----\n\n", getPhaseMetadata(phase).name);
        std::print("{}", FormatIR(*IR, true, true));
    }
}

void Debugger::Notify(Phase phase)
{
    PhaseMetadata phaseMeta = getPhaseMetadata(phase);

    if (RecievedNotifications.contains(phase))
        return;

    if (phaseMeta.isTOKPhase)
        PrintIR(mTokenStream, phase);

    else if (phaseMeta.isASTPhase)
        PrintIR(mAST, phase);

    else if (phaseMeta.isCFGPhase)
        PrintIR(mCFG, phase);

    else if (phaseMeta.isTACPhase || (phaseMeta.isTACPhase && phaseMeta.isOptimizationPhase))
        PrintIR(mTAC, phase);

    else if (phaseMeta.isMIRPhase || (phaseMeta.isMIRPhase && phaseMeta.isOptimizationPhase))
        PrintIR(mMIR, phase);

    else if (phaseMeta.isASMPhase)
        PrintIR(&mASMFilePath, phase);

    RecievedNotifications.insert(phase);
}

void Debugger::Reset()
{
    mTokenStream = nullptr;
    mAST = nullptr;
    mCFG = nullptr;
    mTAC = nullptr;
    mMIR = nullptr;
    mASMFilePath = "";

    RecievedNotifications.clear();
};
