#include "CompareElimination.h"
#include "Globals.h"
#include "IRCommon.h"
#include "IREditor.h"
#include "MIRInstructions.h"
#include "IRFormatters.h"
#include <format>
#include <optional>

bool writesFlags(MIRInstruction* inst)
{
    switch (inst->type)
    {
        case MIRInstType::ADD:
        case MIRInstType::SUB:
        case MIRInstType::MUL:
        case MIRInstType::DIV:
        case MIRInstType::NEG:
        case MIRInstType::SHL:
        case MIRInstType::SAR:
        case MIRInstType::XOR:
        case MIRInstType::CMP:
        case MIRInstType::TEST:
        case MIRInstType::CALL:
            return true;

        default:
            return false;
    }
}

void EliminateCompareInstructions(MIR& MIR)
{
    for (auto& MIRFunc : MIR)
    {
        for (auto& block : MIRFunc->Blocks)
        {
            MIRInstruction* flagOperandInst = nullptr;
            std::optional<Operand> flagsOf = std::nullopt;

            for (size_t i = 0; i < block->Instructions.size(); i++)
            {
                MIRInstruction* inst = block->Instructions[i].get();

                MIRInstOperands operands = GetMIRInstOperands(inst);

                Operand* compared = nullptr;

                if (inst->type == MIRInstType::CMP)
                {
                    MIRCmp* cmp = static_cast<MIRCmp*>(inst);

                    if (std::holds_alternative<Immediate>(cmp->Right) && std::get<Immediate>(cmp->Right).Value == 0)
                        compared = &cmp->Left;
                }

                else if (inst->type == MIRInstType::TEST)
                {
                    MIRTest* test = static_cast<MIRTest*>(inst);

                    if (test->Left == test->Right)
                        compared = &test->Left;
                }

                if (compared != nullptr && flagsOf != std::nullopt && *compared == *flagsOf)
                {
                    std::vector<std::pair<Condition*, MIRInstruction*>> readers;

                    bool rewritable = true;

                    for (size_t j = i + 1; j < block->Instructions.size() && !writesFlags(block->Instructions[j].get()); j++)
                    {
                        MIRInstruction* next = block->Instructions[j].get();

                        Condition* cond = nullptr;

                        if (next->type == MIRInstType::CJMP)
                            cond = &static_cast<MIRCondJump*>(next)->Cond;

                        else if (next->type == MIRInstType::CMOV)
                            cond = &static_cast<MIRCmov*>(next)->Cond;

                        else if (next->type == MIRInstType::SET)
                            cond = &static_cast<MIRSet*>(next)->Cond;

                        if (!cond)
                            continue;

                        readers.push_back(std::make_pair(cond, next));

                        rewritable = rewritable && (*cond == Condition::EQUAL || *cond == Condition::NOT_EQUAL || *cond == Condition::LESS || *cond == Condition::GREATER_EQUAL || *cond == Condition::SIGN || *cond == Condition::NOT_SIGN);
                    }

                    if (rewritable == true)
                    {
                        for (auto [cond, readerInst] : readers)
                        {
                            if (*cond == Condition::LESS)
                            {
                                *cond = Condition::SIGN;

                                IREditor<MIRTypes>::addMessage(readerInst, Message{Phase::CMP_ELIM, IRTransformType::MODIFIED, std::format("changed 'less than' condition in instruction '{}' to 'is signed' condition '{}'", FormatMIRInstruction(readerInst, false).substr(4), FormatMIRInstruction(readerInst, false).substr(4))});
                            }

                            else if (*cond == Condition::GREATER_EQUAL)
                            {
                                *cond = Condition::NOT_SIGN;

                                IREditor<MIRTypes>::addMessage(readerInst, Message{Phase::CMP_ELIM, IRTransformType::MODIFIED, std::format("changed 'greater than or equal to' condition in instruction '{}' to 'is not signed' condition '{}'", FormatMIRInstruction(readerInst, false).substr(4), FormatMIRInstruction(readerInst, false).substr(4))});
                            }
                        }

                        IREditor<MIRTypes>::deleteInstruction(block.get(), inst, Message{Phase::CMP_ELIM, IRTransformType::DELETED, std::format("deleted redundant comparison over '{}' after '{}'", OperandString(*flagsOf), FormatMIRInstruction(flagOperandInst, false).substr(4))});

                        i--;
                    }

                    continue;
                }

                if (writesFlags(inst))
                {
                    flagsOf = std::nullopt;

                    bool isProducer = inst->type == MIRInstType::ADD || inst->type == MIRInstType::SUB || inst->type == MIRInstType::NEG || inst->type == MIRInstType::XOR;

                    if (isProducer && (std::holds_alternative<Register>(*operands.Def) || std::holds_alternative<VirtualRegister>(*operands.Def)))
                    {
                        flagOperandInst = inst;
                        flagsOf = *operands.Def;
                    }
                }

                else if (flagsOf != std::nullopt)
                {
                    std::vector<Operand> defs(operands.ImplicitDefs.begin(), operands.ImplicitDefs.end());

                    if (operands.Def != nullptr)
                        defs.push_back(*operands.Def);

                    if (std::find(defs.begin(), defs.end(), *flagsOf) != defs.end())
                        flagsOf = std::nullopt;
                }
            }
        }
    }
}
