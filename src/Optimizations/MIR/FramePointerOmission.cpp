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

                switch (inst->type)
                {
                    case MIRInstType::MOV:
                        {
                            MIRMov* mov = static_cast<MIRMov*>(inst);

                            AdjustOffset(mov->Dest);
                            AdjustOffset(mov->Source);
                        }
                        break;

                    case MIRInstType::ADD:
                        {
                            MIRAdd* add = static_cast<MIRAdd*>(inst);

                            AdjustOffset(add->Dest);
                            AdjustOffset(add->Source);
                        }
                        break;

                    case MIRInstType::SUB:
                        {
                            MIRSub* sub = static_cast<MIRSub*>(inst);

                            AdjustOffset(sub->Dest);
                            AdjustOffset(sub->Source);
                        }
                        break;

                    case MIRInstType::MUL:
                        {
                            MIRImul* mul = static_cast<MIRImul*>(inst);

                            AdjustOffset(mul->Dest);
                            AdjustOffset(mul->Source);
                        }
                        break;

                    case MIRInstType::DIV:
                        {
                            MIRIdiv* div = static_cast<MIRIdiv*>(inst);

                            AdjustOffset(div->Divisor);
                        }
                        break;

                    case MIRInstType::NEG:
                        {
                            MIRNeg* neg = static_cast<MIRNeg*>(inst);

                            AdjustOffset(neg->Dest);
                        }
                        break;

                    case MIRInstType::CMP:
                        {
                            MIRCmp* cmp = static_cast<MIRCmp*>(inst);

                            AdjustOffset(cmp->Left);
                            AdjustOffset(cmp->Right);
                        }
                        break;

                    case MIRInstType::TEST:
                        {
                            MIRTest* test = static_cast<MIRTest*>(inst);

                            AdjustOffset(test->Left);
                            AdjustOffset(test->Right);
                        }
                        break;

                    // MIR generator and virtual register resolvers dont emit ANY push instructions in between a function, so this push is guaranteed to be from the prologue...
                    case MIRInstType::PUSH:
                        {
                            MIRInstruction* prologueMov = Block->Instructions[i + 1].get();
                            MIRInstruction* prologueSub = Block->Instructions[i + 2].get();

                            IREditor<MIRTypes>::deleteInstruction(Block.get(), inst, Message{Phase::FPO, IRTransformType::DELETED, "prologue instruction redundant after stack offsets made relative to rsp"});

                            IREditor<MIRTypes>::deleteInstruction(Block.get(), prologueMov, Message{Phase::FPO, IRTransformType::DELETED, "prologue instruction redundant after stack offsets made relative to rsp"});

                            if (MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128)
                                IREditor<MIRTypes>::deleteInstruction(Block.get(), prologueSub, Message{Phase::FPO, IRTransformType::DELETED, std::format("stack frame adjustment unnecessary for leaf function with stack frame size of {}(less than 128)", MIRFunc->StackFrameSize)});

                            i--;
                        }
                        break;

                    // MIR generator and virtual register resolvers dont emit ANY pop instructions in between a function, so this pop is guaranteed to be from the epilogue...
                    case MIRInstType::POP:
                        {
                            MIRInstruction* epilogueMov = Block->Instructions[i - 1].get();

                            IREditor<MIRTypes>::deleteInstruction(Block.get(), epilogueMov, Message{Phase::FPO, IRTransformType::DELETED, "epilogue instruction redundant after stack offsets made relative to rsp"});

                            if (MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128)
                                IREditor<MIRTypes>::deleteInstruction(Block.get(), inst, Message{Phase::FPO, IRTransformType::DELETED, std::format("stack frame adjustment unnecessary for leaf function with stack frame size of {}(less than 128)", MIRFunc->StackFrameSize)});
                            else
                                IREditor<MIRTypes>::replaceInstruction(Block.get(), inst, std::make_unique<MIRAdd>(Register::RSP, Immediate{MIRFunc->StackFrameSize}), Message{Phase::FPO, IRTransformType::REPLACED, std::format("changed 'pop rbp' to 'add rsp, {}'", MIRFunc->StackFrameSize)});
                        }
                        break;
                }
            }
        }
    }
}
