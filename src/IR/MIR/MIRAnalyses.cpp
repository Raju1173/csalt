#include "MIRAnalyses.h"
#include "MIRGenerator.h"

MIRUseDefInfo& MIRFunction::getUseDefInfo()
{
    MIRUseDefInfo& UseDefInfo = Metadata.UseDefInfo;

    for (auto& MIRBlock : Blocks)
    {
        for (auto& inst : MIRBlock->Instructions)
        {
            //
        }
    }

    return UseDefInfo;
}
