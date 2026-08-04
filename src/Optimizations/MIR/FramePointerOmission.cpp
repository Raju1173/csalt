#include "MIRGenerator.h"
#include "MIREditor.h"
#include "MIRInstructions.h"
#include <memory>
#include <variant>
#include <vector>

void OmitFramePointers(MIR& MIR)
{
    auto AdjustOffset = [](Operand& Op, MIRFunction* MIRFunc) {
        if (std::holds_alternative<StackOffset>(Op))
        {
            std::get<StackOffset>(Op).RSP = true;

            if (std::get<StackOffset>(Op).Offset > 0)
                std::get<StackOffset>(Op).Offset -= 8;

            if (!(MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128))
                std::get<StackOffset>(Op).Offset += MIRFunc->StackFrameSize;
        }
    };

    for (auto& MIRFunc : MIR)
    {
        for (int i = MIRFunc->Blocks.size(); i >= 0; i--)
        {
            auto& Block = MIRFunc->Blocks[i];

            for (int i = Block->Instructions.size(); i >= 0; i--)
            {
                auto& inst = Block->Instructions[i];

                switch (inst->type)
                {
                    case MIRType::MOV:
                        {
                            MIRMov* mov = static_cast<MIRMov*>(inst.get());

                            AdjustOffset(mov->Dest, MIRFunc.get());
                            AdjustOffset(mov->Source, MIRFunc.get());
                        }
                        break;

                    case MIRType::ADD:
                        {
                            MIRAdd* add = static_cast<MIRAdd*>(inst.get());

                            AdjustOffset(add->Dest, MIRFunc.get());
                            AdjustOffset(add->Source, MIRFunc.get());
                        }
                        break;

                    case MIRType::SUB:
                        {
                            MIRSub* sub = static_cast<MIRSub*>(inst.get());

                            AdjustOffset(sub->Dest, MIRFunc.get());
                            AdjustOffset(sub->Source, MIRFunc.get());
                        }
                        break;

                    case MIRType::MUL:
                        {
                            MIRImul* mul = static_cast<MIRImul*>(inst.get());

                            AdjustOffset(mul->Dest, MIRFunc.get());
                            AdjustOffset(mul->Source, MIRFunc.get());
                        }
                        break;

                    case MIRType::DIV:
                        {
                            MIRIdiv* div = static_cast<MIRIdiv*>(inst.get());

                            AdjustOffset(div->Divisor, MIRFunc.get());
                        }
                        break;

                    case MIRType::NEG:
                        {
                            MIRNeg* neg = static_cast<MIRNeg*>(inst.get());

                            AdjustOffset(neg->Dest, MIRFunc.get());
                        }
                        break;

                    case MIRType::CMP:
                        {
                            MIRCmp* cmp = static_cast<MIRCmp*>(inst.get());

                            AdjustOffset(cmp->Left, MIRFunc.get());
                            AdjustOffset(cmp->Right, MIRFunc.get());
                        }
                        break;

                    // MIR generator and virtual register resolvers dont emit ANY push instructions in between a function, so this push is guaranteed to be from the prologue...
                    case MIRType::PUSH:
                        {
                            MIREditor::deleteInstruction(Block.get(), inst.get());

                            MIREditor::deleteInstruction(Block.get(), Block->Instructions[i - 1].get());

                            if (MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128)
                                MIREditor::deleteInstruction(Block.get(), Block->Instructions[i - 2].get());
                        }
                        break;

                    // MIR generator and virtual register resolvers dont emit ANY pop instructions in between a function, so this pop is guaranteed to be from the epilogue...
                    case MIRType::POP:
                        {
                            if (MIRFunc->IsLeaf && MIRFunc->StackFrameSize <= 128)
                                MIREditor::deleteInstruction(Block.get(), inst.get());
                            else
                                MIREditor::replaceInstruction(Block.get(), inst.get(), std::make_unique<MIRAdd>(Register::RSP, Immediate{MIRFunc->StackFrameSize}));

                            MIREditor::deleteInstruction(Block.get(), Block->Instructions[i - 1].get());
                        }
                        break;
                }
            }
        }
    }
}
