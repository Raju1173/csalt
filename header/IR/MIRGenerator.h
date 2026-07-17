#pragma once

#include "TACGenerator.h"
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

struct StackOffset
{
    int Offset;
};

struct Immediate
{
    int Value;
};

using Operand = std::variant<Register, StackOffset, Immediate>;

std::string OperandString(const Operand& op, bool UseRSP);

class MIRInstruction
{
public:
    MIRType type;

    MIRInstruction(MIRType type) : type(type){};

    virtual ~MIRInstruction() = default;
};

class MIRMov : public MIRInstruction
{
public:
    MIRMov() : MIRInstruction(MIRType::MOV){};

    Operand Dest;
    Operand Source;
};

class MIRAdd : public MIRInstruction
{
public:
    MIRAdd() : MIRInstruction(MIRType::ADD) {}

    Operand Dest;
    Operand Source;
};

class MIRSub : public MIRInstruction
{
public:
    MIRSub() : MIRInstruction(MIRType::SUB) {}

    Operand Dest;
    Operand Source;
};

class MIRImul : public MIRInstruction
{
public:
    MIRImul() : MIRInstruction(MIRType::MUL) {}

    Operand Dest;
    Operand Source;
};

class MIRIdiv : public MIRInstruction
{
public:
    MIRIdiv() : MIRInstruction(MIRType::DIV) {}

    Operand Divisor;
};

class MIRNeg : public MIRInstruction
{
public:
    MIRNeg() : MIRInstruction(MIRType::NEG) {}

    Operand Dest;
};

class MIRShl : public MIRInstruction
{
public:
    MIRShl() : MIRInstruction(MIRType::SHL) {}

    Operand Dest;
    Operand Count;
};

class MIRSar : public MIRInstruction
{
public:
    MIRSar() : MIRInstruction(MIRType::SAR) {}

    Operand Dest;
    Operand Count;
};

class MIRLea : public MIRInstruction
{
public:
    MIRLea() : MIRInstruction(MIRType::LEA) {}

    Operand Dest;
    Operand Base;
    Operand Index;
    Operand Scale;
};

class MIRCmp : public MIRInstruction
{
public:
    MIRCmp() : MIRInstruction(MIRType::CMP) {}

    Operand Left;
    Operand Right;
};

class MIRJump : public MIRInstruction
{
public:
    MIRJump() : MIRInstruction(MIRType::JMP) {}

    size_t TargetBlock;
};

class MIRPush : public MIRInstruction
{
public:
    MIRPush() : MIRInstruction(MIRType::PUSH) {}

    Operand Source;
};

class MIRPop : public MIRInstruction
{
public:
    MIRPop() : MIRInstruction(MIRType::POP) {}

    Operand Dest;
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
    MIRCondJump() : MIRInstruction(MIRType::CJMP) {}

    Condition Cond;
    size_t TargetBlock;
};

class MIRCall : public MIRInstruction
{
public:
    MIRCall() : MIRInstruction(MIRType::CALL) {}

    std::string Function;
};

class MIRRet : public MIRInstruction
{
public:
    MIRRet() : MIRInstruction(MIRType::RET) {}
};

class MIRCdq : public MIRInstruction
{
public:
    MIRCdq() : MIRInstruction(MIRType::CDQ) {}
};

struct MIRFunction;

struct MIRBlock
{
    size_t ID;

    MIRFunction* Function;

    std::vector<std::unique_ptr<MIRInstruction>> Instructions;
};

struct MIRFunction
{
    std::string FunctionName;

    std::vector<std::string> Parameters;

    std::vector<MIRBlock> Blocks;

    int StackFrameSize = 0;

    bool OmitFramePtr = false;

    // gets flipped during MIRgeneration...
    bool IsLeaf = true;
};

using MIR = std::vector<std::unique_ptr<MIRFunction>>;

MIR GenerateMachineIR(TAC& TAC);

void PrintMIR(MIR& MIR);
