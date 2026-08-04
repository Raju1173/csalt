#pragma once

#include "MIRInstructions.h"
#include <unordered_map>
#include <vector>

class MIRBlock;
class MIRFunction;

struct MIRUseDefInfo
{
    bool isValid = false;

    std::unordered_map<MIRInstruction*, std::vector<Operand*>> Uses;
    std::unordered_map<MIRInstruction*, std::vector<Operand*>> Defs;
};

struct MIRLivenessInfo
{
    bool isValid = false;

    //
};

struct MIRMetaData
{
    MIRUseDefInfo UseDefInfo;
    MIRLivenessInfo LivenessInfo;
};
