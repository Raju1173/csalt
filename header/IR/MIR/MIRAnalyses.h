#pragma once

#include "MIRInstructions.h"
#include <unordered_map>
#include <vector>

class MIRBlock;
class MIRFunction;

struct MIROperandLiveInterval
{
    VirtualRegister* vreg;

    int start;
    int end;
};

struct MIRLivenessInfo
{
    bool isValid = false;

    std::vector<MIROperandLiveInterval> Liveness;
};

struct MIRMetaData
{
    MIRLivenessInfo LivenessInfo;
};
