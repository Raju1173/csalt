#pragma once

#include "CFGBuilder.h"
#include <optional>
#include <unordered_set>
#include <variant>
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
    NEG,
    BRANCH,
    SELECT,
    CALL,
    RETURN
};

enum class TACPass
{
    CONSTANT_FOLDING,
    ALGEBRAIC_SIMPLIFICATION,
    BRANCH_SIMPLIFICATION,
    CONTROL_FLOW_SIMPLIFICATION,
    DCE,
    GVN,
    LICM,
    CTFE
};

enum class InstTransformType
{
    DELETED,

    MOVED,
    CLONED,

    REPLACED,
    REPLACED_OPERAND,
    SIMPLIFIED,

    RENAMED
};

std::string TransformationToStr(InstTransformType op)
{
    switch (op)
    {
        case InstTransformType::MOVED:
            return "MOVED";
        case InstTransformType::CLONED:
            return "CLONED";
        case InstTransformType::REPLACED:
            return "REPLACED";
        case InstTransformType::REPLACED_OPERAND:
            return "REPLACED_OP";
        case InstTransformType::SIMPLIFIED:
            return "SIMPLIFIED";
        case InstTransformType::DELETED:
            return "DELETED";
        case InstTransformType::RENAMED:
            return "RENAMED";
    }
}

class TACBlock;

struct Message
{
    TACPass Pass;
    InstTransformType TranformationType;
    std::string Info;
};

class TACInstruction
{
public:
    const TACType type;

    bool dead = false;

    std::vector<Message> History;

    TACInstruction(TACType type) : type(type){};

    virtual ~TACInstruction() = default;
};

struct TACVariable
{
    std::string OriginalName;
    std::string SSAName = this->OriginalName;
};

using TACValue = std::variant<int, TACVariable>;

inline std::string TACValToStr(const TACValue& val)
{
    if (val.index() == 0)
    {
        return std::to_string(std::get<int>(val));
    }

    const auto& var = std::get<TACVariable>(val);
    return var.SSAName.empty() ? var.OriginalName : var.SSAName;
}

class TACBinaryOp : public TACInstruction
{
public:
    TACBinaryOp() : TACInstruction(TACType::BINARYOP){};

    TACValue dest;

    TACValue left;
    BinaryOp op;
    TACValue right;
};

class TACNeg : public TACInstruction
{
public:
    TACNeg() : TACInstruction(TACType::NEG){};

    TACValue dest;

    TACValue source;
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
    TACJump() : TACInstruction(TACType::JUMP), TargetBlock(){};

    TACBlock* TargetBlock;
};

struct PhiArgument
{
    size_t SourceID;
    TACValue Value;
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

    TACBlock* TrueTarget;
    TACBlock* FalseTarget;
};

class TACSelect : public TACInstruction
{
public:
    TACSelect() : TACInstruction(TACType::SELECT){};

    TACValue dest;

    Comparison cond;
    TACValue TrueVal;
    TACValue FalseVal;
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

    std::optional<TACValue> ReturnValue = std::nullopt;
};

class TACFunction;

class TACBlock
{
public:
    size_t ID;

    TACFunction* Function;

    std::vector<std::unique_ptr<TACInstruction>> Instructions;

    std::vector<TACBlock*> Parents;
    std::vector<TACBlock*> Children;

    TACBlock(size_t ID, TACFunction* function) : ID(ID), Function(function){};
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

struct TACFrontierInfo
{
    bool isValid = false;

    std::unordered_map<TACBlock*, std::vector<TACBlock*>> Frontiers;
};

struct TACVarUsesInfo
{
    bool isValid = false;

    std::unordered_map<TACValue, size_t> VarUses;
};

struct TACLoop
{
    TACBlock* Header;
    TACBlock* End;
    std::unordered_set<TACBlock*> Blocks;
};

struct TACDefBlocksInfo
{
    bool isValid = false;

    std::unordered_map<TACValue, std::unordered_set<TACBlock*>> DefBlocks;
};

struct TACLoopInfo
{
    bool isValid = false;

    std::vector<TACLoop> Loops;
};

struct TACMetaData
{
    TACDominatorInfo DomInfo;

    TACDominatorTreeInfo DomTreeInfo;

    TACFrontierInfo FrontierInfo;

    TACVarUsesInfo VarUsesInfo;

    TACDefBlocksInfo DefBlocksInfo;

    TACLoopInfo LoopInfo;
};

class TACFunction
{
public:
    std::string Name;

    std::vector<std::string> Parameters;

    std::vector<std::unique_ptr<TACBlock>> Blocks;

    TACFunction(std::string name, std::vector<std::string> parameters) : Name(name), Parameters(parameters){};

private:
    TACMetaData Metadata;

public:
    TACDominatorInfo& getDominatorInfo();

    TACDominatorTreeInfo& getDominatorTreeInfo();

    TACFrontierInfo& getFrontierInfo();

    TACVarUsesInfo& getVarUsesInfo();

    TACDefBlocksInfo& getDefBlocksInfo();

    TACLoopInfo& getLoopInfo();
};

using TAC = std::vector<std::unique_ptr<TACFunction>>;

TAC GenerateTAC(CFG& CFG);

void ResolvePhiNodes(TAC& TAC);

void PrintTAC(TAC& TAC);
