#pragma once

#include "MIRInstructions.h"
#include <unordered_map>
#include <vector>

class MIRBlock;
class MIRFunction;

struct MIRLivenessInfo
{
    bool isValid = false;

    //
};

struct MIRMetaData
{
    MIRLivenessInfo LivenessInfo;
};
