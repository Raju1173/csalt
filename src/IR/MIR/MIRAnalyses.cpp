#include "MIRAnalyses.h"
#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include <algorithm>
#include <unordered_map>
#include <variant>

MIRLivenessInfo& MIRFunction::getLivenessInfo()
{
    MIRLivenessInfo& LivenessInfo = Metadata.LivenessInfo;

    if (!LivenessInfo.isValid)
    {
        LivenessInfo.Liveness.clear();
        LivenessInfo.isValid = true;

        std::unordered_map<VirtualRegister*, MIROperandLiveInterval> liveMap;

        int instructionIndex = 0;

        for (auto& block : Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                auto processVReg = [&liveMap, &instructionIndex](VirtualRegister* vreg) {
                    if (liveMap.find(vreg) == liveMap.end())
                    {
                        liveMap[vreg] = {vreg, instructionIndex, instructionIndex};
                    }

                    else
                    {
                        liveMap[vreg].end = instructionIndex;
                    }
                };

                if (GetMIRInstOperands(inst.get()).Def != nullptr && std::get_if<VirtualRegister>(GetMIRInstOperands(inst.get()).Def) != nullptr)
                {
                    processVReg(&std::get<VirtualRegister>(*GetMIRInstOperands(inst.get()).Def));
                }

                for (Operand* use : GetMIRInstOperands(inst.get()).Uses)
                {
                    if (std::get_if<VirtualRegister>(use) != nullptr)
                        processVReg(&std::get<VirtualRegister>(*use));
                }

                inst->id = instructionIndex++;
            }
        }

        LivenessInfo.Liveness.reserve(liveMap.size());

        for (auto& [vreg, interval] : liveMap)
        {
            LivenessInfo.Liveness.push_back(interval);
        }

        std::sort(LivenessInfo.Liveness.begin(), LivenessInfo.Liveness.end(), [](MIROperandLiveInterval& a, MIROperandLiveInterval& b) { return a.start < b.start; });
    }

    return LivenessInfo;
}
