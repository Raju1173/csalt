#include "IRCommon.h"
#include "IREditor.h"
#include "TACGenerator.h"
#include "TACInstructions.h"
#include <memory>

bool SimplifyPhiNodes(TAC& TAC)
{
    bool changed = false;

    for (auto& TACFunc : TAC)
    {
        for (auto& TACBlock : TACFunc->Blocks)
        {
            for (size_t i = 0; i < TACBlock->Instructions.size(); i++)
            {
                auto& inst = TACBlock->Instructions[i];

                if (inst->type != TACInstType::PHI)
                    continue;

                TACPhi* phi = static_cast<TACPhi*>(inst.get());

                if (phi->args.size() == 1)
                {
                    IREditor<TACTypes>::replaceInstruction(TACBlock.get(), phi, std::make_unique<TACAssign>(phi->variable, phi->args[0].Value), Message{});
                    changed = true;
                    continue;
                }

                TACValue* commonValue = nullptr;

                bool isTrivial = true;

                for (auto& arg : phi->args)
                {
                    if (arg.Value == phi->variable)
                        continue;

                    if (commonValue == nullptr)
                    {
                        commonValue = &arg.Value;
                    }

                    else if (commonValue != &arg.Value)
                    {
                        isTrivial = false;
                        break;
                    }
                }

                if (isTrivial && commonValue != nullptr)
                {
                    IREditor<TACTypes>::replaceInstruction(TACBlock.get(), phi, std::make_unique<TACAssign>(phi->variable, *commonValue), Message{});
                    changed = true;
                }
            }
        }
    }

    return changed;
}
