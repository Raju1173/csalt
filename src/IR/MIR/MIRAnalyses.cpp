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

        std::unordered_map<int, MIROperandLiveInterval> liveMap;

        int instructionIndex = 0;

        for (auto& block : Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                auto processVReg = [&liveMap, instructionIndex](VirtualRegister& vreg) {
                    if (liveMap.find(vreg.ID) == liveMap.end())
                    {
                        liveMap[vreg.ID] = {&vreg, instructionIndex, instructionIndex};
                    }

                    else
                    {
                        liveMap[vreg.ID].end = instructionIndex;
                    }
                };

                MIRInstOperands operands = GetMIRInstOperands(inst.get());

                if (operands.Def != nullptr && std::get_if<VirtualRegister>(operands.Def) != nullptr)
                {
                    processVReg(std::get<VirtualRegister>(*operands.Def));
                }

                for (Operand* use : operands.Uses)
                {
                    if (std::get_if<VirtualRegister>(use) != nullptr)
                    {
                        processVReg(std::get<VirtualRegister>(*use));
                    }
                }

                inst->id = instructionIndex++;
            }
        }

        LivenessInfo.Liveness.reserve(liveMap.size());

        for (auto& [id, interval] : liveMap)
        {
            LivenessInfo.Liveness.push_back(interval);
        }

        std::sort(LivenessInfo.Liveness.begin(), LivenessInfo.Liveness.end(), [](MIROperandLiveInterval& a, MIROperandLiveInterval& b) { return a.start < b.start; });
    }

    return LivenessInfo;
}
