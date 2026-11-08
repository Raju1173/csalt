#pragma once

#include "TACGenerator.h"
#include "IREditor.h"
#include "MIRAnalyses.h"
#include "MIRInstructions.h"
#include <memory>
#include <vector>

template<typename IRTypes> class IREditor;

class MIRBlock
{
public:
    size_t ID;

    MIRFunction* Function;

    std::vector<std::unique_ptr<MIRInstruction>> Instructions;

    std::vector<MIRBlock*> Parents;
    std::vector<MIRBlock*> Children;

    std::vector<Message> History;

    DeadSiblings<MIRBlock> deadSiblings;

    std::unique_ptr<MIRInstruction> LastDeadInstruction = nullptr;

    MIRBlock(size_t ID, MIRFunction* function) : ID(ID), Function(function){};

    template<typename IRTypes> friend class IREditor;
};

class MIRFunction
{
public:
    std::string FunctionName;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<MIRBlock>> Blocks;

    int StackFrameSize = 0;

    // gets flipped during MIRgeneration...
    bool IsLeaf = true;

    std::vector<Message> History;

    DeadSiblings<MIRFunction> deadSiblings;

    std::unique_ptr<MIRBlock> LastDeadBlock;

    size_t NextBlockID = 1;

    MIRFunction(std::string name, std::vector<std::string> parameters) : FunctionName(name), Parameters(parameters){};

private:
    MIRMetaData Metadata;

public:
    MIRLivenessInfo& getLivenessInfo();

    template<typename IRTypes> friend class IREditor;
};

using MIR = std::vector<std::unique_ptr<MIRFunction>>;

void GenerateMachineIR(TAC& TAC, MIR& MIR);

// this function had to be moved here from MIRInstructions.h due to cyclic dependencies...
inline MIRInstOperands GetMIRInstOperands(MIRInstruction* inst)
{
    static_assert(std::to_underlying(MIRInstType::COUNT) == 22, "Add another case and increment the count check when adding a new MIRInstType!!!");

    switch (inst->type)
    {
        case MIRInstType::MOV:
            {
                MIRMov* mov = static_cast<MIRMov*>(inst);
                return {&mov->Dest, {&mov->Source}};
            }
        case MIRInstType::MOVZX:
            {
                MIRMovzx* movzx = static_cast<MIRMovzx*>(inst);
                return {&movzx->Dest, {&movzx->Source}};
            }
        case MIRInstType::CMOV:
            {
                MIRCmov* cmov = static_cast<MIRCmov*>(inst);
                return {&cmov->Dest, {&cmov->Dest, &cmov->Source}};
            }
        case MIRInstType::ADD:
            {
                MIRAdd* add = static_cast<MIRAdd*>(inst);
                return {&add->Dest, {&add->Dest, &add->Source}};
            }
        case MIRInstType::SUB:
            {
                MIRSub* sub = static_cast<MIRSub*>(inst);
                return {&sub->Dest, {&sub->Dest, &sub->Source}};
            }
        case MIRInstType::MUL:
            {
                MIRImul* mul = static_cast<MIRImul*>(inst);
                return {&mul->Dest, {&mul->Dest, &mul->Source}};
            }
        case MIRInstType::DIV:
            {
                MIRIdiv* div = static_cast<MIRIdiv*>(inst);
                return {nullptr, {&div->Divisor}, {Register::EAX, Register::EDX}, {Register::EAX, Register::EDX}};
            }
        case MIRInstType::NEG:
            {
                MIRNeg* neg = static_cast<MIRNeg*>(inst);
                return {&neg->Dest, {&neg->Dest}};
            }
        case MIRInstType::SHL:
            {
                MIRShl* shl = static_cast<MIRShl*>(inst);
                return {&shl->Dest, {&shl->Dest, &shl->Count}};
            }
        case MIRInstType::SAR:
            {
                MIRSar* sar = static_cast<MIRSar*>(inst);
                return {&sar->Dest, {&sar->Dest, &sar->Count}};
            }
        case MIRInstType::LEA:
            {
                MIRLea* lea = static_cast<MIRLea*>(inst);
                return {&lea->Dest, {&lea->Base, &lea->Index, &lea->Scale}};
            }
        case MIRInstType::XOR:
            {
                MIRXor* Xor = static_cast<MIRXor*>(inst);
                return {&Xor->Dest, {&Xor->Dest, &Xor->Source}};
            }
        case MIRInstType::CMP:
            {
                MIRCmp* cmp = static_cast<MIRCmp*>(inst);
                return {nullptr, {&cmp->Left, &cmp->Right}};
            }
        case MIRInstType::TEST:
            {
                MIRTest* test = static_cast<MIRTest*>(inst);
                return {nullptr, {&test->Left, &test->Right}};
            }
        case MIRInstType::POP:
            {
                MIRPop* pop = static_cast<MIRPop*>(inst);
                return {&pop->Dest, {}};
            }
        case MIRInstType::PUSH:
            {
                MIRPush* push = static_cast<MIRPush*>(inst);
                return {nullptr, {&push->Source}};
            }

        case MIRInstType::SET:
            {
                MIRSet* set = static_cast<MIRSet*>(inst);
                return {&set->Dest, {}};
            }

        case MIRInstType::CALL:
            {
                MIRCall* call = static_cast<MIRCall*>(inst);
                MIRInstOperands ops;

                size_t n = std::min<size_t>(call->Function->Parameters.size(), 6);

                for (size_t i = 0; i < n; i++)
                {
                    ops.ImplicitUses.insert(ArgRegs[i]);
                }

                ops.ImplicitDefs = {Register::EAX, Register::EDI, Register::ESI, Register::EDX, Register::ECX, Register::R8D, Register::R9D, Register::R10D, Register::R11D};

                return ops;
            }

        case MIRInstType::CDQ:
            return {nullptr, {}, {Register::EAX}, {Register::EDX}};

        case MIRInstType::RET:
            return {nullptr, {}, {Register::EAX}, {}};

        case MIRInstType::CJMP:
            return {};
        case MIRInstType::JMP:
            return {};
    }

    return {};
}
