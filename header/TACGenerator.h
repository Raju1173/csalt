#pragma once

#include "CFGBuilder.h"
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

constexpr std::string_view BinaryOpToStr[] = {
    "+",
    "-",
    "*",
    "/",

    "==",
    "!=",

    "<",
    "<=",

    ">",
    ">="};

class TACInstruction
{
public:
    virtual ~TACInstruction() = default;
};

struct TACValue
{
    std::string value;

    bool neg = false;

    auto operator<=>(const TACValue&) const = default;
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

    size_t TargetBlock;
};

class TACPhi : public TACInstruction
{
public:
    TACPhi(TACValue var) : variable(std::move(var)){};

    TACValue variable;

    std::vector<PhiArgument> args;
};

struct Comparison
{
    TACValue Left;
    BinaryOp Op;
    TACValue Right;
};

class TACBranch : public TACInstruction
{
public:
    TACBranch(Comparison cond) : cond(cond){};

    Comparison cond;

    size_t TrueTarget;
    size_t FalseTarget;
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
    size_t ID;

    std::vector<std::unique_ptr<TACInstruction>> Instructions;

    std::vector<TACBlock*> Parents;
    std::vector<TACBlock*> Children;

    std::set<TACBlock*> Dominators;
    std::vector<TACBlock*> DominatorTreeChildren;
};

struct TACFunction
{
    std::string Name;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<TACBlock>> Blocks;
};

std::vector<std::unique_ptr<TACFunction>> GenerateTAC(std::vector<std::unique_ptr<CFGFunction>>& CFG);

void ResolvePhiNodes(std::vector<std::unique_ptr<TACFunction>>& TAC);

void printTAC(std::vector<std::unique_ptr<TACFunction>>& TAC);
