#include "IRDebugger.h"
#include "MIRGenerator.h"
#include "RegisterAllocator.h"

void ResolveVRegsLinearScan(MIR& MIR)
{
    for (auto& MIRFunc : MIR)
    {
        MIRLivenessInfo& LivenessInfo = MIRFunc->getLivenessInfo();

        for (auto& MIRBlock : MIRFunc->Blocks)
        {
            for (auto& inst : MIRBlock->Instructions)
            {
                //
            }
        }
    }

    Debugger::Notify(Phase::MIR_REG_ALLOC);
}
