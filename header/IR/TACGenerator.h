#pragma once

#include "CFGBuilder.h"
#include <algorithm>
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

enum class TACType
{
    BINARYOP,
    ASSIGN,
    JUMP,
    PHI,
    BRANCH,
    CALL,
    RETURN
};

class TACInstruction
{
public:
    const TACType type;

    TACInstruction(TACType type) : type(type){};

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
    TACBinaryOp() : TACInstruction(TACType::BINARYOP){};

    TACValue dest;

    TACValue left;
    BinaryOp op;
    TACValue right;
};

class TACAssign : public TACInstruction
{
public:
    TACAssign() : TACInstruction(TACType::ASSIGN){};

    TACValue dest;

    TACValue source;
};

class TACJump : public TACInstruction
{
public:
    TACJump(size_t target) : TACInstruction(TACType::JUMP), TargetBlock(target){};

    size_t TargetBlock;
};

class TACPhi : public TACInstruction
{
public:
    TACPhi(TACValue var) : TACInstruction(TACType::PHI), variable(std::move(var)){};

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
    TACBranch(Comparison cond) : TACInstruction(TACType::BRANCH), cond(cond){};

    Comparison cond;

    size_t TrueTarget;
    size_t FalseTarget;
};

class TACCall : public TACInstruction
{
public:
    TACCall() : TACInstruction(TACType::CALL){};

    std::optional<TACValue> dest;

    std::string functionName;

    std::vector<TACValue> args;
};

class TACReturn : public TACInstruction
{
public:
    TACReturn() : TACInstruction(TACType::RETURN){};

    TACValue ReturnValue;
};

class TACBlock
{
public:
    size_t ID;

    std::vector<std::unique_ptr<TACInstruction>> Instructions;

    std::vector<TACBlock*> Parents;
    std::vector<TACBlock*> Children;

    TACBlock(size_t ID) : ID(ID){};

    TACBlock(TACBlock&&) noexcept = default;
    TACBlock& operator=(TACBlock&&) noexcept = default;

    ~TACBlock()
    {
        for (TACBlock* p : Parents)
            std::erase(p->Children, this);

        for (TACBlock* c : Children)
            std::erase(c->Parents, this);
    };
};

struct TACFunction
{
    std::string Name;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<TACBlock>> Blocks;
};

struct TACDominatorInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, std::unordered_set<TACBlock*>> Dominators;
};

struct TACDominatorTreeInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, std::vector<TACBlock*>> DominatorTree;
};

struct TACVarUsesInfo
{
    bool isValid = false;

    std::unordered_map<std::string, size_t> VarUses;
};

struct TACMetaData
{
    TACDominatorInfo DomInfo;

    TACDominatorTreeInfo DomTreeInfo;

    TACVarUsesInfo VarUsesInfo;
};

class TAC
{
private:
    std::vector<TACFunction> Functions;

    std::unordered_map<std::string, TACMetaData> MetaData;

public:
    TACDominatorInfo& getDominatorInfo(std::string FuncName)
    {
        return MetaData[FuncName].DomInfo;
    }

    TACDominatorTreeInfo& getDominatorTreeInfo(std::string FuncName)
    {
        return MetaData[FuncName].DomTreeInfo;
    }

    TACVarUsesInfo& getVarUsesInfo(std::string FuncName)
    {
        return MetaData[FuncName].VarUsesInfo;
    }

    TACDominatorInfo& computeDominators(TACFunction& TACFunc);
    void computeDominators();

    TACDominatorTreeInfo& computeDominatorTree(TACFunction& TACFunc);
    void computeDominatorTree();

    TACVarUsesInfo& computeVarUses(TACFunction& TACFunc);
    void computeVarUses();

    size_t size() const
    {
        return Functions.size();
    }

    TACFunction& operator[](size_t index)
    {
        return Functions[index];
    }

    const TACFunction& operator[](size_t index) const
    {
        return Functions[index];
    }

    void push_back(TACFunction& TACFunction)
    {
        Functions.push_back(std::move(TACFunction));
    }

    void push_back(TACFunction&& TACFunction)
    {
        Functions.push_back(std::move(TACFunction));
    }

    TACFunction& back()
    {
        return Functions.back();
    }

    const TACFunction& back() const
    {
        return Functions.back();
    }

    auto begin() { return Functions.begin(); }
    auto end() { return Functions.end(); }

    auto begin() const { return Functions.begin(); }
    auto end() const { return Functions.end(); }

    template<typename Predicate> void eraseFuncIf(Predicate pred)
    {
        std::erase_if(Functions, [&](const auto& TACFunc) {
            if (pred(TACFunc))
            {
                MetaData.erase(TACFunc.Name);
                return true;
            }

            return false;
        });
    }

    TACFunction& findFuncByName(std::string targetName)
    {
        return *std::ranges::find_if(Functions, [&targetName](std::string& name) { return name == targetName; }, &TACFunction::Name);
    }
};

TAC GenerateTAC(CFG& CFG);

void ResolvePhiNodes(TAC& TAC);

void PrintTAC(TAC& TAC);

bool isConstant(std::string str);
