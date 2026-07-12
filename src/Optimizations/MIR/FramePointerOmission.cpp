#include "MIRGenerator.h"
#include <vector>

void OmitFramePointers(MIR& MIR)
{
    auto AdjustOffset = [](Operand& Op, MIRFunction& MIRFunc) {
        if (Op.index() == 1)
        {
            if (std::get<StackOffset>(Op).Offset > 0)
                std::get<StackOffset>(Op).Offset -= 8;

            if (!(MIRFunc.IsLeaf && MIRFunc.StackFrameSize <= 128))
                std::get<StackOffset>(Op).Offset += MIRFunc.StackFrameSize;
        }
    };

    for (MIRFunction& MIRFunc : MIR)
    {
        MIRFunc.OmitFramePtr = true;

        for (MIRBlock& Block : MIRFunc.Blocks)
        {
            for (size_t i = 0; i < Block.Instructions.size(); i++)
            {
                if (Block.Instructions[i].get() != nullptr)
                {
                    auto& inst = Block.Instructions[i];

                    switch (inst->type)
                    {
                        case MIRType::MOV:
                            {
                                MIRMov* mov = static_cast<MIRMov*>(inst.get());

                                AdjustOffset(mov->Dest, MIRFunc);
                                AdjustOffset(mov->Source, MIRFunc);
                            }
                            break;

                        case MIRType::ADD:
                            {
                                MIRAdd* add = static_cast<MIRAdd*>(inst.get());

                                AdjustOffset(add->Dest, MIRFunc);
                                AdjustOffset(add->Source, MIRFunc);
                            }
                            break;

                        case MIRType::SUB:
                            {
                                MIRSub* sub = static_cast<MIRSub*>(inst.get());

                                AdjustOffset(sub->Dest, MIRFunc);
                                AdjustOffset(sub->Source, MIRFunc);
                            }
                            break;

                        case MIRType::MUL:
                            {
                                MIRImul* mul = static_cast<MIRImul*>(inst.get());

                                AdjustOffset(mul->Dest, MIRFunc);
                                AdjustOffset(mul->Source, MIRFunc);
                            }
                            break;

                        case MIRType::DIV:
                            {
                                MIRIdiv* div = static_cast<MIRIdiv*>(inst.get());

                                AdjustOffset(div->Divisor, MIRFunc);
                            }
                            break;

                        case MIRType::NEG:
                            {
                                MIRNeg* neg = static_cast<MIRNeg*>(inst.get());

                                AdjustOffset(neg->Dest, MIRFunc);
                            }
                            break;

                        case MIRType::CMP:
                            {
                                MIRCmp* cmp = static_cast<MIRCmp*>(inst.get());

                                AdjustOffset(cmp->Left, MIRFunc);
                                AdjustOffset(cmp->Right, MIRFunc);
                            }
                            break;

                        // MIR generator follows zero push mandate, so any push is guaranteed to be from the prologue...
                        case MIRType::PUSH:
                            {
                                inst = std::move(nullptr);

                                Block.Instructions[i + 1] = std::move(nullptr);

                                if (MIRFunc.IsLeaf && MIRFunc.StackFrameSize <= 128)
                                {
                                    Block.Instructions[i + 2] = std::move(nullptr);
                                }
                            }
                            break;

                        // MIR generator follows zero push mandate, so any pop is guaranteed to be from the epilogue...
                        case MIRType::POP:
                            {
                                if (MIRFunc.IsLeaf && MIRFunc.StackFrameSize <= 128)
                                {
                                    inst = std::move(nullptr);
                                }

                                else
                                {
                                    auto add = std::make_unique<MIRAdd>();

                                    add->Dest = Register::RSP;
                                    add->Source = Immediate{MIRFunc.StackFrameSize};

                                    inst = std::move(add);
                                }

                                Block.Instructions[i - 1] = std::move(nullptr);
                            }
                            break;
                    }
                }
            }

            std::erase_if(Block.Instructions, [](auto& inst) { return inst == nullptr; });
        }
    }
}
