#include "TACGenerator.h"

inline TACInstruction* getDefiningInst(TACValue& val, TACDefBlocksInfo& DefBlocksInfo)
{
    if (DefBlocksInfo.DefBlocks[val].empty())
        return nullptr;

    TACBlock* defBlock = *DefBlocksInfo.DefBlocks[val].begin();

    for (auto& blockInst : defBlock->Instructions)
    {
        TACInstOperands operands = GetTACInstOperands(blockInst.get());

        if (operands.Def != nullptr && *operands.Def == val)
            return blockInst.get();
    }

    return nullptr;
}
