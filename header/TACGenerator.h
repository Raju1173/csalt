#pragma once

#include "SSAConstructor.h"
#include <vector>
#include <memory>

enum class BinaryOp
{
    PLUS,
    MINUS,
    MUL,
    DIV,

    DOUBLE_EQUAL,
    NOT_EQUAL,

    LESS,
    LESS_EQUAL,

    GREATER,
    GREATER_EQUAL
};

class TACInstruction
{
public:
    virtual ~TACInstruction() = default;
};

struct TACValue
{
    std::string value;
};

class TACBinaryOp : public TACInstruction
{
public:
    TACValue dest;

    TACValue left;
    BinaryOp op;
    TACValue right;
};

class TACAssign : public TACInstruction
{
public:
    TACValue dest;

    TACValue source;
};

class TACJump : public TACInstruction
{
public:
    TACJump(int target) : TargetBlock(target){};

    int TargetBlock;
};

class TACPhi : public TACInstruction
{
public:
    TACPhi(TACValue var) : variable(std::move(var)){};

    TACValue variable;

    std::vector<PhiArgument> args;
};

class TACBranch : public TACInstruction
{
public:
    TACBranch(TACValue cond) : Condition(std::move(cond)){};

    TACValue Condition;

    int TrueTarget;
    int FalseTarget;
};

class TACCall : public TACInstruction
{
public:
    std::optional<TACValue> dest;

    std::string functionName;

    std::vector<TACValue> args;
};

class TACReturn : public TACInstruction
{
public:
    TACValue ReturnValue;
};

struct TACBlock
{
    int ID;

    std::vector<std::unique_ptr<TACInstruction>> Instructions;
};

struct TACFunction
{
    std::string Name;

    std::vector<std::unique_ptr<TACBlock>> Blocks;
};

std::vector<std::unique_ptr<TACFunction>> GenerateTAC(std::vector<std::unique_ptr<CFGFunction>> &CFG);

void printTAC(std::vector<std::unique_ptr<TACFunction>> &TAC);
