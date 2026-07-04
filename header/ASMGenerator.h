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

struct StackOffset
{
    int Offset;
};

struct Immediate
{
    int Value;
};

using Operand = std::variant<Register, StackOffset, Immediate>;

class MIRInstruction
{
public:
    virtual ~MIRInstruction() = default;
};

class MIRMov : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;
};

class MIRAdd : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;
};

class MIRSub : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;
};

class MIRImul : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;
};

class MIRIdiv : public MIRInstruction
{
public:
    Operand Divisor;
};

class MIRCmp : public MIRInstruction
{
public:
    Operand Left;
    Operand Right;
};

class MIRJump : public MIRInstruction
{
public:
    int TargetBlock;
};

class MIRPush : public MIRInstruction
{
public:
    Operand Source;
};

class MIRPop : public MIRInstruction
{
public:
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
    Condition Cond;
    int TargetBlock;
};

class MIRCall : public MIRInstruction
{
public:
    std::string Function;
};

// clang-format off

class MIRRet : public MIRInstruction {};

class MIRCdq : public MIRInstruction {};

// clang-format on

struct MIRBlock
{
    size_t ID;

    std::vector<std::unique_ptr<MIRInstruction>> Instructions;
};

struct MIRFunction
{
    std::string FunctionName;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<MIRBlock>> Blocks;

    int StackFrameSize = 0;
};

std::vector<std::unique_ptr<MIRFunction>> GenerateMachineIR(std::vector<std::unique_ptr<TACFunction>>& TAC);

void EmitAssembly(const std::vector<std::unique_ptr<MIRFunction>>& MIR, const std::string& filename);
