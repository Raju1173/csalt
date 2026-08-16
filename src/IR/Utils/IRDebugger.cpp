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

template<typename R> void Debugger::TakeSnapshot(R* IR, Phase phase)
{
    if (gCompilerOptions[phase].Dump)
    {
        StaticIRSnapshots.push_back(IRSnapshot{getPhaseMetadata(phase).name, FormatIR(*IR)});
        std::print("-----{}-----\n\n", StaticIRSnapshots.back().Name);
        std::print("{}", StaticIRSnapshots.back().Snapshot);
    }

    if (gCompilerOptions[phase].HistoryDump)
    {
        StaticIRSnapshots.push_back(IRSnapshot{getPhaseMetadata(phase).name, FormatIR(*IR, true)});
        std::print("-----{}-----\n\n", StaticIRSnapshots.back().Name);
        std::print("{}", StaticIRSnapshots.back().Snapshot);
    }

    if (gCompilerOptions[phase].InteractiveDump || gCompilerOptions[phase].InteractiveHistoryDump)
        InteractiveIRSnapshots.push_back(IRSnapshot{getPhaseMetadata(phase).name, FormatIR(*IR, gCompilerOptions[phase].InteractiveHistoryDump)});
}

void Debugger::Notify(Phase phase)
{
    PhaseMetadata phaseMeta = getPhaseMetadata(phase);

    if (RecievedNotifications.contains(phase))
    {
        if (phaseMeta.isOptimizationPhase)
        {
            if (gCompilerOptions[phase].InteractiveDump || gCompilerOptions[phase].InteractiveHistoryDump)
            {
                if (phaseMeta.isTACPhase)
                    InteractiveIRSnapshots.push_back(IRSnapshot{phaseMeta.name, FormatIR(*mTAC, gCompilerOptions[phase].InteractiveHistoryDump)});
                else if (phaseMeta.isMIRPhase)
                    InteractiveIRSnapshots.push_back(IRSnapshot{phaseMeta.name, FormatIR(*mMIR, gCompilerOptions[phase].InteractiveHistoryDump)});
            }
        }

        return;
    }

    if (phaseMeta.isTOKPhase)
        TakeSnapshot(mTokenStream, phase);

    else if (phaseMeta.isASTPhase)
        TakeSnapshot(mAST, phase);

    else if (phaseMeta.isCFGPhase)
        TakeSnapshot(mCFG, phase);

    else if (phaseMeta.isTACPhase || (phaseMeta.isTACPhase && phaseMeta.isOptimizationPhase))
        TakeSnapshot(mTAC, phase);

    else if (phaseMeta.isMIRPhase || (phaseMeta.isMIRPhase && phaseMeta.isOptimizationPhase))
        TakeSnapshot(mMIR, phase);

    else if (phaseMeta.isASMPhase)
        TakeSnapshot(&mASMFilePath, phase);

    RecievedNotifications.insert(phase);
}

void Debugger::Run()
{
    if (!InteractiveIRSnapshots.empty())
    {
        int i = 0;

        std::string prevInput = "n";

        while (true)
        {
            std::print("\033[2J\033[3J\033[1;1H");
            std::print("-----{}-----\n\n", InteractiveIRSnapshots[i].Name);
            std::print("{}", InteractiveIRSnapshots[i].Snapshot);
            std::print("\nSnapshot : {}/{}\n", i + 1, InteractiveIRSnapshots.size());
            std::print("\nControls : [n/p [N]] - next/prev N times, [q] - quit, [Enter] - repeat\n\n");
            std::print("(csalt) ");

            std::string input;
            std::getline(std::cin, input);

            if (input.empty())
                input = prevInput;
            else
                prevInput = input;

            std::istringstream iss(input);
            std::string cmd;
            int steps = 1;

            iss >> cmd;
            if (iss >> steps)
            {
                if (steps < 0)
                    steps = 0;
            }

            if (cmd == "n" || cmd == "next")
            {
                i = std::min(i + steps, static_cast<int>(InteractiveIRSnapshots.size()) - 1);
            }

            else if (cmd == "p" || cmd == "prev")
            {
                i = std::max(i - steps, 0);
            }

            else if (cmd == "q" || cmd == "quit")
            {
                std::print("\033[2J\033[3J\033[1;1H");
                break;
            }
        }
    }

    for (IRSnapshot Snapshot : StaticIRSnapshots)
    {
        if (!InteractiveIRSnapshots.empty())
        {
            std::print("-----{}-----\n\n", Snapshot.Name);
            std::print("{}", Snapshot.Snapshot);
        }
    }
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

    StaticIRSnapshots.clear();
    InteractiveIRSnapshots.clear();
};
