#pragma once

#include "CFGBuilder.h"
#include "Globals.h"
#include "IRFormatters.h"
#include "MIRGenerator.h"
#include "TACGenerator.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

struct IRSnapshot
{
    std::string Name;
    std::string Snapshot;
};

class Debugger
{
private:
    inline static TokenStream* mTokenStream = nullptr;
    inline static Node* mAST = nullptr;
    inline static CFG* mCFG = nullptr;
    inline static TAC* mTAC = nullptr;
    inline static MIR* mMIR = nullptr;
    inline static std::string mASMFilePath = "";

    inline static std::unordered_set<Phase> RecievedNotifications;

    template<typename R> static void PrintIR(R* IR, Phase phase);

public:
    static void AddIR(TokenStream& TokenStream);
    static void AddIR(Node& AST);
    static void AddIR(CFG& CFG);
    static void AddIR(TAC& TAC);
    static void AddIR(MIR& MIR);
    static void AddIR(std::string ASMFilePath);

    static void Notify(Phase phase);

    static void Reset();
};
