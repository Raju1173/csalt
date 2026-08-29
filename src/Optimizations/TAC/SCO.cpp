#include "SCO.h"
#include "TACGenerator.h"
#include "TACInstructions.h"

void MarkSiblingCalls(TAC& TAC)
{
    for (auto& TACFunc : TAC)
    {
        for (auto& TACBlock : TACFunc->Blocks)
        {
            for (size_t i = 0; i < TACBlock->Instructions.size(); i++)
            {
                auto& inst = TACBlock->Instructions[i];

                if (inst->type == TACInstType::CALL)
                {
                    if (i + 1 >= TACBlock->Instructions.size() || TACBlock->Instructions[i + 1]->type != TACInstType::RETURN)
                        continue;

                    TACCall* call = static_cast<TACCall*>(inst.get());
                    TACReturn* ret = static_cast<TACReturn*>(TACBlock->Instructions[i + 1].get());

                    if ((call->dest.has_value() && ret->ReturnValue.has_value() && ret->ReturnValue.value() == call->dest.value()) || (!call->dest.has_value() && !ret->ReturnValue.has_value()))
                    {
                        if (std::max(0, static_cast<int>(TACFunc->Parameters.size()) - 6) >= std::max(0, static_cast<int>(call->args.size()) - 6) && call->functionName != TACFunc->Name)
                        {
                            call->isSiblingCall = true;
                        }
                    }
                }
            }
        }
    }
}
