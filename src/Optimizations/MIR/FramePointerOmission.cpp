#include "IRCommon.h"
#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include <memory>
#include <variant>
#include <vector>

void OmitFramePointers(MIR& MIR)
{
    for (auto& MIRFunc : MIR)
    {
        auto AdjustOffset = [&MIRFunc](Operand& Op) {
            if (std::holds_alternative<StackOffset>(Op) && !std::get<StackOffset>(Op).RSP)
            {
                std::get<StackOffset>(Op).RSP = true;

                if (std::get<StackOffset>(Op).Offset > 0)
                    std::get<StackOffset>(Op).Offset -= 8;

                if (!(MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128))
                    std::get<StackOffset>(Op).Offset += MIRFunc->StackFrameSize;
            }
        };

        for (auto& Block : MIRFunc->Blocks)
        {
            for (int i = 0; i < Block->Instructions.size(); i++)
            {
                MIRInstruction* inst = Block->Instructions[i].get();

                // MIR generator and virtual register resolvers dont emit ANY push instructions in between a function, so this push is guaranteed to be from the prologue...
                if (inst->type == MIRInstType::PUSH)
                {
                    MIRInstruction* prologueMov = Block->Instructions[i + 1].get();
                    MIRInstruction* prologueSub = Block->Instructions[i + 2].get();

                    IREditor<MIRTypes>::deleteInstruction(Block.get(), inst, Message{Phase::FPO, IRTransformType::DELETED, "prologue instruction redundant after stack offsets made relative to rsp"});

                    IREditor<MIRTypes>::deleteInstruction(Block.get(), prologueMov, Message{Phase::FPO, IRTransformType::DELETED, "prologue instruction redundant after stack offsets made relative to rsp"});

                    if (MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128)
                        IREditor<MIRTypes>::deleteInstruction(Block.get(), prologueSub, Message{Phase::FPO, IRTransformType::DELETED, std::format("stack frame adjustment unnecessary for leaf function with stack frame size of {}(less than 128)", MIRFunc->StackFrameSize)});

                    i--;

                    continue;
                }

                // MIR generator and virtual register resolvers dont emit ANY pop instructions in between a function, so this pop is guaranteed to be from the epilogue...
                else if (inst->type == MIRInstType::POP)
                {
                    MIRInstruction* epilogueMov = Block->Instructions[i - 1].get();

                    IREditor<MIRTypes>::deleteInstruction(Block.get(), epilogueMov, Message{Phase::FPO, IRTransformType::DELETED, "epilogue instruction redundant after stack offsets made relative to rsp"});

                    if (MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128)
                        IREditor<MIRTypes>::deleteInstruction(Block.get(), inst, Message{Phase::FPO, IRTransformType::DELETED, std::format("stack frame adjustment unnecessary for leaf function with stack frame size of {}(less than 128)", MIRFunc->StackFrameSize)});
                    else
                        IREditor<MIRTypes>::replaceInstruction(Block.get(), inst, std::make_unique<MIRAdd>(Register::RSP, Immediate{MIRFunc->StackFrameSize}), Message{Phase::FPO, IRTransformType::REPLACED, std::format("changed 'pop rbp' to 'add rsp, {}'", MIRFunc->StackFrameSize)});

                    continue;
                }

                for (Operand* operand : GetMIRInstOperands(inst).Operands)
                {
                    AdjustOffset(*operand);
                }
            }
        }
    }
}
