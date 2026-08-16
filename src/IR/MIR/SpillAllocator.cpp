#include "IRDebugger.h"
#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include "RegisterAllocator.h"
#include <memory>
#include <variant>

void ResolveVRegsSpillAll(MIR& MIR)
{
    for (auto& MIRFunc : MIR)
    {
        std::unordered_map<int, int> vregMap;
        int currentOffset = 8;

        auto resolveOperand = [&currentOffset, &vregMap](Operand& op) {
            if (std::holds_alternative<VirtualRegister>(op))
            {
                if (!vregMap.contains(std::get<VirtualRegister>(op).ID))
                {
                    vregMap[std::get<VirtualRegister>(op).ID] = currentOffset;
                    currentOffset += 8;
                }

                op = StackOffset{-vregMap[std::get<VirtualRegister>(op).ID]};
            }
        };

        for (auto& block : MIRFunc->Blocks)
        {
            for (size_t i = 0; i < block->Instructions.size(); ++i)
            {
                MIRInstruction* inst = block->Instructions[i].get();

                if (inst->type == MIRInstType::MOV)
                {
                    MIRMov* mov = static_cast<MIRMov*>(inst);

                    resolveOperand(mov->Dest);
                    resolveOperand(mov->Source);

                    if (std::holds_alternative<StackOffset>(mov->Dest) && std::holds_alternative<StackOffset>(mov->Source))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, mov->Source));
                        i++;
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRMov>(mov->Dest, Register::R10D));
                    }
                }

                else if (inst->type == MIRInstType::ADD)
                {
                    MIRAdd* add = static_cast<MIRAdd*>(inst);

                    resolveOperand(add->Dest);
                    resolveOperand(add->Source);

                    if (std::holds_alternative<StackOffset>(add->Dest) && std::holds_alternative<StackOffset>(add->Source))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, add->Source));
                        i++;
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRAdd>(add->Dest, Register::R10D));
                    }
                }

                else if (inst->type == MIRInstType::SUB)
                {
                    MIRSub* sub = static_cast<MIRSub*>(inst);

                    resolveOperand(sub->Dest);
                    resolveOperand(sub->Source);

                    if (std::holds_alternative<StackOffset>(sub->Dest) && std::holds_alternative<StackOffset>(sub->Source))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, sub->Source));
                        i++;
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRSub>(sub->Dest, Register::R10D));
                    }
                }

                else if (inst->type == MIRInstType::MUL)
                {
                    MIRImul* mul = static_cast<MIRImul*>(inst);

                    resolveOperand(mul->Dest);
                    resolveOperand(mul->Source);

                    if (std::holds_alternative<StackOffset>(mul->Dest))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, mul->Dest));
                        i++;
                        IREditor<MIRTypes>::addInstructionAfter(block.get(), inst, std::make_unique<MIRMov>(mul->Dest, Register::R10D));
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRImul>(Register::R10D, mul->Source));
                    }
                }

                else if (inst->type == MIRInstType::DIV)
                {
                    MIRIdiv* div = static_cast<MIRIdiv*>(inst);

                    resolveOperand(div->Divisor);
                }

                else if (inst->type == MIRInstType::NEG)
                {
                    MIRNeg* neg = static_cast<MIRNeg*>(inst);

                    resolveOperand(neg->Dest);
                }

                else if (inst->type == MIRInstType::XOR)
                {
                    MIRXor* xorInst = static_cast<MIRXor*>(inst);

                    resolveOperand(xorInst->Dest);
                    resolveOperand(xorInst->Source);

                    if (std::holds_alternative<StackOffset>(xorInst->Dest) && std::holds_alternative<StackOffset>(xorInst->Source))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, xorInst->Source));
                        i++;
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRXor>(xorInst->Dest, Register::R10D));
                    }
                }

                else if (inst->type == MIRInstType::CMP)
                {
                    MIRCmp* cmp = static_cast<MIRCmp*>(inst);

                    resolveOperand(cmp->Left);
                    resolveOperand(cmp->Right);

                    if (std::holds_alternative<StackOffset>(cmp->Left) && std::holds_alternative<StackOffset>(cmp->Right))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, cmp->Left));
                        i++;
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRCmp>(Register::R10D, cmp->Right));
                    }
                }

                else if (inst->type == MIRInstType::TEST)
                {
                    MIRTest* test = static_cast<MIRTest*>(inst);

                    resolveOperand(test->Left);
                    resolveOperand(test->Right);

                    if (std::holds_alternative<StackOffset>(test->Left) && std::holds_alternative<StackOffset>(test->Right))
                    {
                        IREditor<MIRTypes>::addInstructionBefore(block.get(), inst, std::make_unique<MIRMov>(Register::R10D, test->Left));
                        i++;
                        IREditor<MIRTypes>::replaceInstruction(block.get(), inst, std::make_unique<MIRTest>(Register::R10D, test->Right));
                    }
                }
            }
        }

        MIRSub* frameAdjustmentInst = static_cast<MIRSub*>(MIRFunc->Blocks.front()->Instructions[2].get());

        MIRFunc->StackFrameSize = (vregMap.size() * 8 + std::get<Immediate>(frameAdjustmentInst->Source).Value + 15) & ~15;

        IREditor<MIRTypes>::replaceInstruction(MIRFunc->Blocks.front().get(), frameAdjustmentInst, std::make_unique<MIRSub>(Register::RSP, Immediate{MIRFunc->StackFrameSize}));
    }

    Debugger::Notify(Phase::MIR_REG_ALLOC);
}
