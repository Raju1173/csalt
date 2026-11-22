#include "RegisterCopyPropagation.h"
#include "Globals.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "MIRInstructions.h"

bool PropagateRegisterCopies(MIR& MIR)
{
    bool changed = false;

    auto isCopyable = [](Operand op) -> bool {
        if (std::holds_alternative<VirtualRegister>(op))
            return true;

        if (!std::holds_alternative<Register>(op))
            return false;

        if (!gCompilerOptions[Phase::FPO].enabled && std::get<Register>(op) == Register::RBP)
            return false;

        return std::get<Register>(op) != Register::RSP && std::get<Register>(op) != Register::R10B;
    };

    for (auto& MIRFunc : MIR)
    {
        for (auto& block : MIRFunc->Blocks)
        {
            std::vector<std::pair<Operand, Operand>> copies;

            for (size_t i = 0; i < block->Instructions.size(); i++)
            {
                MIRInstruction* inst = block->Instructions[i].get();
                MIRInstOperands operands = GetMIRInstOperands(inst);

                Operand* fixedCount = nullptr;

                if (inst->type == MIRInstType::SHL)
                    fixedCount = &static_cast<MIRShl*>(inst)->Count;

                else if (inst->type == MIRInstType::SAR)
                    fixedCount = &static_cast<MIRSar*>(inst)->Count;

                for (Operand* use : operands.Uses)
                {
                    if (use == operands.Def || use == fixedCount || !isCopyable(*use))
                        continue;

                    for (auto& [dest, source] : copies)
                    {
                        if (*use == dest)
                        {
                            IREditor<MIRTypes>::addMessage(inst, Message{Phase::REG_CPROP, IRTransformType::MODIFIED, std::format("replaced use of {} with {}", OperandString(*use), OperandString(source))});

                            *use = source;

                            IREditor<MIRTypes>::invalidateDataFlowAnalyses(MIRFunc->Metadata);

                            changed = true;

                            break;
                        }
                    }
                }

                MIRMov* mov = inst->type == MIRInstType::MOV ? static_cast<MIRMov*>(inst) : nullptr;
                bool isCopy = mov && isCopyable(mov->Dest) && isCopyable(mov->Source);

                if (isCopy && (mov->Dest == mov->Source || std::find(copies.begin(), copies.end(), std::pair{mov->Dest, mov->Source}) != copies.end()))
                {
                    IREditor<MIRTypes>::deleteInstruction(block.get(), inst, Message{Phase::REG_CPROP, IRTransformType::DELETED, "deleted redundant copy"});

                    changed = true;

                    i--;

                    continue;
                }

                std::vector<Operand> defs(operands.ImplicitDefs.begin(), operands.ImplicitDefs.end());

                if (operands.Def)
                    defs.push_back(*operands.Def);

                for (Operand& def : defs)
                    std::erase_if(copies, [&def](auto& copy) { return copy.first == def || copy.second == def; });

                if (isCopy)
                    copies.push_back({mov->Dest, mov->Source});
            }
        }
    }

    return changed;
}
