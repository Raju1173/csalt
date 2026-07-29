#pragma once

#include "IRCommon.h"
#include <memory>
#include <variant>
#include <vector>

enum class Register
{
    EAX,

    EDI,
    ESI,
    EDX,
    ECX,
    R8D,
    R9D,

    R10D,
    R11D,

    EBX,
    R12D,
    R13D,
    R14D,
    R15D,

    RSP,
    RBP
};

std::string RegisterName(Register reg);

struct VirtualRegister
{
    int ID;
};

struct StackOffset
{
    int Offset;
};

struct Immediate
{
    int Value;
};

using Operand = std::variant<Register, VirtualRegister, StackOffset, Immediate>;

std::string OperandString(const Operand& op, bool UseRSP);

enum class MIRType
{
    MOV,
    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    SHL,
    SAR,
    LEA,
    CMP,
    JMP,
    POP,
    CDQ,
    RET,
    PUSH,
    CJMP,
    CALL
};

class MIRBlock;
class MIRFunction;

class MIRInstruction
{
public:
    MIRType type;

    std::vector<Message> History;

    DeadSiblings<MIRInstruction> deadSiblings;

    MIRInstruction(MIRType type) : type(type){};

    virtual ~MIRInstruction() = default;
};

class MIRMov : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRMov() : MIRInstruction(MIRType::MOV){};
    MIRMov(Operand dest, Operand source) : MIRInstruction(MIRType::MOV), Dest(dest), Source(source){};
};

class MIRAdd : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRAdd() : MIRInstruction(MIRType::ADD){};
    MIRAdd(Operand dest, Operand source) : MIRInstruction(MIRType::ADD), Dest(dest), Source(source){};
};

class MIRSub : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRSub() : MIRInstruction(MIRType::SUB){};
    MIRSub(Operand dest, Operand source) : MIRInstruction(MIRType::SUB), Dest(dest), Source(source){};
};

class MIRImul : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRImul() : MIRInstruction(MIRType::MUL){};
    MIRImul(Operand dest, Operand source) : MIRInstruction(MIRType::MUL), Dest(dest), Source(source){};
};

class MIRIdiv : public MIRInstruction
{
public:
    Operand Divisor;

    MIRIdiv() : MIRInstruction(MIRType::DIV){};
    MIRIdiv(Operand divisor) : MIRInstruction(MIRType::DIV), Divisor(divisor){};
};

class MIRNeg : public MIRInstruction
{
public:
    Operand Dest;

    MIRNeg() : MIRInstruction(MIRType::NEG){};
    MIRNeg(Operand dest) : MIRInstruction(MIRType::NEG), Dest(dest){};
};

class MIRShl : public MIRInstruction
{
public:
    Operand Dest;
    Operand Count;

    MIRShl() : MIRInstruction(MIRType::SHL){};
    MIRShl(Operand dest, Operand count) : MIRInstruction(MIRType::SHL), Dest(dest), Count(count){};
};

class MIRSar : public MIRInstruction
{
public:
    Operand Dest;
    Operand Count;

    MIRSar() : MIRInstruction(MIRType::SAR){};
    MIRSar(Operand dest, Operand count) : MIRInstruction(MIRType::SAR), Dest(dest), Count(count){};
};

class MIRLea : public MIRInstruction
{
public:
    Operand Dest;
    Operand Base;
    Operand Index;
    Operand Scale;

    MIRLea() : MIRInstruction(MIRType::LEA){};
    MIRLea(Operand dest, Operand base, Operand index, Operand scale) : MIRInstruction(MIRType::LEA), Dest(dest), Base(base), Index(index), Scale(scale){};
};

class MIRCmp : public MIRInstruction
{
public:
    Operand Left;
    Operand Right;

    MIRCmp() : MIRInstruction(MIRType::CMP){};
    MIRCmp(Operand left, Operand right) : MIRInstruction(MIRType::CMP), Left(left), Right(right){};
};

class MIRJump : public MIRInstruction
{
public:
    MIRBlock* TargetBlock;

    MIRJump() : MIRInstruction(MIRType::JMP){};
    MIRJump(MIRBlock* targetBlock) : MIRInstruction(MIRType::JMP), TargetBlock(targetBlock){};
};

class MIRPush : public MIRInstruction
{
public:
    Operand Source;

    MIRPush() : MIRInstruction(MIRType::PUSH){};
    MIRPush(Operand source) : MIRInstruction(MIRType::PUSH), Source(source){};
};

class MIRPop : public MIRInstruction
{
public:
    Operand Dest;

    MIRPop() : MIRInstruction(MIRType::POP){};
    MIRPop(Operand dest) : MIRInstruction(MIRType::POP), Dest(dest){};
};

enum class Condition
{
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL
};

class MIRCondJump : public MIRInstruction
{
public:
    Condition Cond;
    MIRBlock* TargetBlock;

    MIRCondJump() : MIRInstruction(MIRType::CJMP){};
    MIRCondJump(Condition cond, MIRBlock* targetBlock) : MIRInstruction(MIRType::CJMP), Cond(cond), TargetBlock(targetBlock){};
};

class MIRCall : public MIRInstruction
{
public:
    MIRFunction* Function;

    MIRCall() : MIRInstruction(MIRType::CALL){};
    MIRCall(MIRFunction* function) : MIRInstruction(MIRType::CALL), Function(function){};
};

class MIRRet : public MIRInstruction
{
public:
    MIRRet() : MIRInstruction(MIRType::RET){};
};

class MIRCdq : public MIRInstruction
{
public:
    MIRCdq() : MIRInstruction(MIRType::CDQ){};
};

inline std::unique_ptr<MIRInstruction> cloneMIRInstruction(MIRInstruction* inst)
{
    if (!inst)
        return nullptr;

    switch (inst->type)
    {
        case MIRType::MOV:
            return std::make_unique<MIRMov>(*static_cast<MIRMov*>(inst));
        case MIRType::ADD:
            return std::make_unique<MIRAdd>(*static_cast<MIRAdd*>(inst));
        case MIRType::SUB:
            return std::make_unique<MIRSub>(*static_cast<MIRSub*>(inst));
        case MIRType::MUL:
            return std::make_unique<MIRImul>(*static_cast<MIRImul*>(inst));
        case MIRType::DIV:
            return std::make_unique<MIRIdiv>(*static_cast<MIRIdiv*>(inst));
        case MIRType::NEG:
            return std::make_unique<MIRNeg>(*static_cast<MIRNeg*>(inst));
        case MIRType::SHL:
            return std::make_unique<MIRShl>(*static_cast<MIRShl*>(inst));
        case MIRType::SAR:
            return std::make_unique<MIRSar>(*static_cast<MIRSar*>(inst));
        case MIRType::LEA:
            return std::make_unique<MIRLea>(*static_cast<MIRLea*>(inst));
        case MIRType::CMP:
            return std::make_unique<MIRCmp>(*static_cast<MIRCmp*>(inst));
        case MIRType::JMP:
            return std::make_unique<MIRJump>(*static_cast<MIRJump*>(inst));
        case MIRType::POP:
            return std::make_unique<MIRPop>(*static_cast<MIRPop*>(inst));
        case MIRType::CDQ:
            return std::make_unique<MIRCdq>(*static_cast<MIRCdq*>(inst));
        case MIRType::RET:
            return std::make_unique<MIRRet>(*static_cast<MIRRet*>(inst));
        case MIRType::PUSH:
            return std::make_unique<MIRPush>(*static_cast<MIRPush*>(inst));
        case MIRType::CJMP:
            return std::make_unique<MIRCondJump>(*static_cast<MIRCondJump*>(inst));
        case MIRType::CALL:
            return std::make_unique<MIRCall>(*static_cast<MIRCall*>(inst));
    }

    return nullptr;
}
