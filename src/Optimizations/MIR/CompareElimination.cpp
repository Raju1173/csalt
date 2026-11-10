#include "CompareElimination.h"
#include "Globals.h"
#include "IRCommon.h"
#include "MIRInstructions.h"
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
                    std::vector<Condition*> readers;

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

                        readers.push_back(cond);

                        rewritable = rewritable && (*cond == Condition::EQUAL || *cond == Condition::NOT_EQUAL || *cond == Condition::LESS || *cond == Condition::GREATER_EQUAL || *cond == Condition::SIGN || *cond == Condition::NOT_SIGN);
                    }

                    if (rewritable == true)
                    {
                        for (Condition* cond : readers)
                        {
                            if (*cond == Condition::LESS)
                                *cond = Condition::SIGN;

                            else if (*cond == Condition::GREATER_EQUAL)
                                *cond = Condition::NOT_SIGN;
                        }

                        IREditor<MIRTypes>::deleteInstruction(block.get(), inst, Message{Phase::CMP_ELIM, IRTransformType::DELETED, ""});

                        i--;
                    }

                    continue;
                }

                if (writesFlags(inst))
                {
                    flagsOf = std::nullopt;

                    bool isProducer = inst->type == MIRInstType::ADD || inst->type == MIRInstType::SUB || inst->type == MIRInstType::NEG || inst->type == MIRInstType::XOR;

                    if (isProducer && (std::holds_alternative<Register>(*operands.Def) || std::holds_alternative<VirtualRegister>(*operands.Def)))
                        flagsOf = *operands.Def;
                }

                else if (inst->type == MIRInstType::CDQ || (flagsOf && operands.Def && *operands.Def == *flagsOf))
                    flagsOf = std::nullopt;
            }
        }
    }
}
