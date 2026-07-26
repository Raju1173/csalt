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
    CTFE,

    SSA_RECONSTRUCTION,
};

inline std::string PassToStr(TACPass pass)
{
    switch (pass)
    {
        case TACPass::CONSTANT_FOLDING:
            return "CONSTANT FOLDING";
        case TACPass::ALGEBRAIC_SIMPLIFICATION:
            return "ALGEBRAIC SIMPLIFICATION";
        case TACPass::BRANCH_SIMPLIFICATION:
            return "BRANCH SIMPLIFICATION";
        case TACPass::CONTROL_FLOW_SIMPLIFICATION:
            return "CONTROL FLOW SIMPLIFICATION";
        case TACPass::DCE:
            return "DEAD CODE ELIMINATION";
        case TACPass::GVN:
            return "GLOBAL VALUE NUMBERING";
        case TACPass::LICM:
            return "LOOP INVARIANT CODE MOTION";
        case TACPass::CTFE:
            return "COMPILE TIME FUNCTION EXECUTION";
        case TACPass::SSA_RECONSTRUCTION:
            return "SSA RECONSTRUCTION";
    }
}

enum class TACTransformType
{
    ADDED,

    DELETED,

    MOVED,
    CLONED,

    REPLACED,
    RENAMED,
};

inline std::string TransformationToStr(TACTransformType op)
{
    switch (op)
    {
        case TACTransformType::MOVED:
            return "MOVED";
        case TACTransformType::CLONED:
            return "CLONED";
        case TACTransformType::REPLACED:
            return "REPLACED";
        case TACTransformType::RENAMED:
            return "RENAMED";
        case TACTransformType::ADDED:
            return "ADDED";
        case TACTransformType::DELETED:
            return "DELETED";
    }
}

class TACBlock;

struct Message
{
    TACPass Pass;
    TACTransformType TranformationType;
    std::string Info;
};

// ignore the questionable naming please...
template<typename T> class DeadSiblings
{
public:
    std::vector<std::unique_ptr<T>> preceding;
    std::vector<std::unique_ptr<T>> trailing;

    DeadSiblings() = default;

    DeadSiblings(DeadSiblings& other){};

    void absorbLeftSibling(std::unique_ptr<T>& deadElement)
    {
        auto deadPreceding = std::move(deadElement->deadSiblings.preceding);
        auto deadTrailing = std::move(deadElement->deadSiblings.trailing);

        std::vector<std::unique_ptr<T>> combined;

        combined.insert(combined.end(), std::make_move_iterator(deadPreceding.begin()), std::make_move_iterator(deadPreceding.end()));

        combined.push_back(std::move(deadElement));

        combined.insert(combined.end(), std::make_move_iterator(deadTrailing.begin()), std::make_move_iterator(deadTrailing.end()));
        combined.insert(combined.end(), std::make_move_iterator(preceding.begin()), std::make_move_iterator(preceding.end()));

        preceding = std::move(combined);
    }

    void absorbRightSibling(std::unique_ptr<T>& deadElement)
    {
        auto deadPreceding = std::move(deadElement->deadSiblings.preceding);
        auto deadTrailing = std::move(deadElement->deadSiblings.trailing);

        std::vector<std::unique_ptr<T>> combined;

        combined.insert(combined.end(), std::make_move_iterator(trailing.begin()), std::make_move_iterator(trailing.end()));
        combined.insert(combined.end(), std::make_move_iterator(deadPreceding.begin()), std::make_move_iterator(deadPreceding.end()));

        combined.push_back(std::move(deadElement));

        combined.insert(combined.end(), std::make_move_iterator(deadTrailing.begin()), std::make_move_iterator(deadTrailing.end()));

        trailing = std::move(combined);
    }
};

class TACInstruction
{
public:
    const TACType type;

    std::vector<Message> History;

    DeadSiblings<TACInstruction> deadSiblings;

    TACInstruction(TACType type) : type(type){};

    virtual ~TACInstruction() = default;
};

struct TACVariable
{
    std::string OriginalName;
    std::string SSAName = this->OriginalName;

    auto operator<=>(const TACVariable&) const = default;
};

namespace std
{
template<> struct hash<TACVariable>
{
    std::size_t operator()(const TACVariable& var) const noexcept
    {
        return std::hash<std::string>{}(var.SSAName);
    }
};
}

using TACValue = std::variant<int, TACVariable>;

inline std::string TACValToStr(const TACValue& val)
{
    if (val.index() == 0)
    {
        return std::to_string(std::get<int>(val));
    }

    const auto& var = std::get<TACVariable>(val);
    return var.SSAName;
}

class TACBinaryOp : public TACInstruction
{
public:
    TACValue dest;
    TACValue left;
    BinaryOp op;
    TACValue right;

    TACBinaryOp() : TACInstruction(TACType::BINARYOP){};

    TACBinaryOp(TACValue dest, TACValue left, BinaryOp op, TACValue right) : TACInstruction(TACType::BINARYOP), dest(std::move(dest)), left(std::move(left)), op(op), right(std::move(right)){};
};

class TACNeg : public TACInstruction
{
public:
    TACValue dest;
    TACValue source;

    TACNeg() : TACInstruction(TACType::NEG){};

    TACNeg(TACValue dest, TACValue source) : TACInstruction(TACType::NEG), dest(std::move(dest)), source(std::move(source)){};
};

class TACAssign : public TACInstruction
{
public:
    TACValue dest;
    TACValue source;

    TACAssign() : TACInstruction(TACType::ASSIGN){};

    TACAssign(TACValue dest, TACValue source) : TACInstruction(TACType::ASSIGN), dest(std::move(dest)), source(std::move(source)){};
};

class TACJump : public TACInstruction
{
public:
    TACBlock* TargetBlock;

    TACJump() : TACInstruction(TACType::JUMP), TargetBlock(nullptr){};

    TACJump(TACBlock* targetBlock) : TACInstruction(TACType::JUMP), TargetBlock(targetBlock){};
};

struct PhiArgument
{
    TACBlock* SourceBlock;
    TACValue Value;
};

class TACPhi : public TACInstruction
{
public:
    TACValue variable;
    std::vector<PhiArgument> args;

    TACPhi() : TACInstruction(TACType::PHI){};

    TACPhi(TACValue variable) : TACInstruction(TACType::PHI), variable(variable){};

    TACPhi(TACValue var, std::vector<PhiArgument> args) : TACInstruction(TACType::PHI), variable(std::move(var)), args(std::move(args)){};
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
    Comparison cond;
    TACBlock* TrueTarget;
    TACBlock* FalseTarget;

    TACBranch() : TACInstruction(TACType::BRANCH), cond(), TrueTarget(nullptr), FalseTarget(nullptr){};

    TACBranch(Comparison cond) : TACInstruction(TACType::BRANCH), cond(cond), TrueTarget(nullptr), FalseTarget(nullptr){};

    TACBranch(Comparison cond, TACBlock* trueTarget, TACBlock* falseTarget) : TACInstruction(TACType::BRANCH), cond(std::move(cond)), TrueTarget(trueTarget), FalseTarget(falseTarget){};
};

class TACSelect : public TACInstruction
{
public:
    TACValue dest;
    Comparison cond;
    TACValue TrueVal;
    TACValue FalseVal;

    TACSelect() : TACInstruction(TACType::SELECT){};

    TACSelect(TACValue dest, Comparison cond, TACValue trueVal, TACValue falseVal) : TACInstruction(TACType::SELECT), dest(std::move(dest)), cond(std::move(cond)), TrueVal(std::move(trueVal)), FalseVal(std::move(falseVal)){};
};

class TACCall : public TACInstruction
{
public:
    std::optional<TACValue> dest;
    std::string functionName;
    std::vector<TACValue> args;

    TACCall() : TACInstruction(TACType::CALL){};

    TACCall(std::optional<TACValue> dest, std::string functionName, std::vector<TACValue> args) : TACInstruction(TACType::CALL), dest(std::move(dest)), functionName(std::move(functionName)), args(std::move(args)){};
};

class TACReturn : public TACInstruction
{
public:
    std::optional<TACValue> ReturnValue = std::nullopt;

    TACReturn() : TACInstruction(TACType::RETURN){};

    TACReturn(std::optional<TACValue> returnValue) : TACInstruction(TACType::RETURN), ReturnValue(std::move(returnValue)){};
};

inline std::unique_ptr<TACInstruction> cloneTACInstruction(TACInstruction* inst)
{
    switch (inst->type)
    {
        case TACType::ASSIGN:
            return std::make_unique<TACAssign>(*static_cast<TACAssign*>(inst));
        case TACType::JUMP:
            return std::make_unique<TACJump>(*static_cast<TACJump*>(inst));
        case TACType::BINARYOP:
            return std::make_unique<TACBinaryOp>(*static_cast<TACBinaryOp*>(inst));
        case TACType::RETURN:
            return std::make_unique<TACReturn>(*static_cast<TACReturn*>(inst));
        case TACType::BRANCH:
            return std::make_unique<TACBranch>(*static_cast<TACBranch*>(inst));
        case TACType::CALL:
            return std::make_unique<TACCall>(*static_cast<TACCall*>(inst));
        case TACType::NEG:
            return std::make_unique<TACNeg>(*static_cast<TACNeg*>(inst));
        case TACType::PHI:
            return std::make_unique<TACPhi>(*static_cast<TACPhi*>(inst));
        case TACType::SELECT:
            return std::make_unique<TACSelect>(*static_cast<TACSelect*>(inst));
    }

    return nullptr;
}

class TACFunction;

class TACBlock
{
public:
    size_t ID;

    TACFunction* Function;

    std::vector<std::unique_ptr<TACInstruction>> Instructions;

    std::vector<TACBlock*> Parents;
    std::vector<TACBlock*> Children;

    std::vector<Message> History;

    DeadSiblings<TACBlock> deadSiblings;

    std::unique_ptr<TACInstruction> LastDeadInstruction = nullptr;

    TACBlock(size_t ID, TACFunction* function) : ID(ID), Function(function){};

    friend class TACEditor;
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

    std::vector<Message> History;

    DeadSiblings<TACFunction> deadSiblings;

    std::unique_ptr<TACBlock> LastDeadBlock;

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

    friend class TACEditor;
};

using TAC = std::vector<std::unique_ptr<TACFunction>>;

TAC GenerateTAC(CFG& CFG);

void PrintTAC(TAC& TAC, bool history = false);
