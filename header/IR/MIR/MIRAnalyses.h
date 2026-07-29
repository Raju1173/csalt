#pragma once

#include "MIRInstructions.h"

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
