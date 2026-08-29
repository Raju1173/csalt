#pragma once

#include "IRCommon.h"
#include <optional>
#include <unordered_map>
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
    GREATER_EQUAL,

    COUNT
};

inline std::string BinaryOpToStr(BinaryOp Op)
{
    static_assert(std::to_underlying(BinaryOp::COUNT) == 10, "Add/Remove the relevant case from the switch when modifying the TokenType enum!!!");

    switch (Op)
    {
        case BinaryOp::PLUS:
            return "+";
        case BinaryOp::MINUS:
            return "-";
        case BinaryOp::MUL:
            return "*";
        case BinaryOp::DIV:
            return "/";

        case BinaryOp::DOUBLE_EQUAL:
            return "==";
        case BinaryOp::NOT_EQUAL:
            return "!=";

        case BinaryOp::LESS:
            return "<";
        case BinaryOp::LESS_EQUAL:
            return "<=";

        case BinaryOp::GREATER:
            return ">";
        case BinaryOp::GREATER_EQUAL:
            return ">=";
    }
};

enum class TACInstType
{
    BINARYOP,
    ASSIGN,
    JUMP,
    PHI,
    NEG,
    BRANCH,
    SELECT,
    CALL,
    RETURN,

    COUNT
};

class TACBlock;

class TACInstruction
{
public:
    const TACInstType type;

    std::vector<Message> History;

    DeadSiblings<TACInstruction> deadSiblings;

    TACInstruction(TACInstType type) : type(type){};

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
    if (std::holds_alternative<int>(val))
    {
        return std::to_string(std::get<int>(val));
    }

    return std::get<TACVariable>(val).SSAName;
}

class TACBinaryOp : public TACInstruction
{
public:
    TACValue dest;
    TACValue left;
    BinaryOp op;
    TACValue right;

    TACBinaryOp() : TACInstruction(TACInstType::BINARYOP){};

    TACBinaryOp(TACValue dest, TACValue left, BinaryOp op, TACValue right) : TACInstruction(TACInstType::BINARYOP), dest(dest), left(left), op(op), right(right){};
};

class TACNeg : public TACInstruction
{
public:
    TACValue dest;
    TACValue source;

    TACNeg() : TACInstruction(TACInstType::NEG){};

    TACNeg(TACValue dest, TACValue source) : TACInstruction(TACInstType::NEG), dest(dest), source(source){};
};

class TACAssign : public TACInstruction
{
public:
    TACValue dest;
    TACValue source;

    TACAssign() : TACInstruction(TACInstType::ASSIGN){};

    TACAssign(TACValue dest, TACValue source) : TACInstruction(TACInstType::ASSIGN), dest(dest), source(source){};
};

class TACJump : public TACInstruction
{
public:
    TACBlock* TargetBlock;

    TACJump() : TACInstruction(TACInstType::JUMP), TargetBlock(nullptr){};

    TACJump(TACBlock* targetBlock) : TACInstruction(TACInstType::JUMP), TargetBlock(targetBlock){};
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

    TACPhi() : TACInstruction(TACInstType::PHI){};

    TACPhi(TACValue variable) : TACInstruction(TACInstType::PHI), variable(variable){};

    TACPhi(TACValue var, std::vector<PhiArgument> args) : TACInstruction(TACInstType::PHI), variable(var), args(args){};
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

    TACBranch() : TACInstruction(TACInstType::BRANCH), cond(), TrueTarget(nullptr), FalseTarget(nullptr){};

    TACBranch(Comparison cond) : TACInstruction(TACInstType::BRANCH), cond(cond), TrueTarget(nullptr), FalseTarget(nullptr){};

    TACBranch(Comparison cond, TACBlock* trueTarget, TACBlock* falseTarget) : TACInstruction(TACInstType::BRANCH), cond(cond), TrueTarget(trueTarget), FalseTarget(falseTarget){};
};

class TACSelect : public TACInstruction
{
public:
    TACValue dest;
    Comparison cond;
    TACValue TrueVal;
    TACValue FalseVal;

    TACSelect() : TACInstruction(TACInstType::SELECT){};

    TACSelect(TACValue dest, Comparison cond, TACValue trueVal, TACValue falseVal) : TACInstruction(TACInstType::SELECT), dest(dest), cond(cond), TrueVal(trueVal), FalseVal(falseVal){};
};

class TACCall : public TACInstruction
{
public:
    std::optional<TACValue> dest;
    std::string functionName;
    std::vector<TACValue> args;

    bool isSiblingCall = false;

    TACCall() : TACInstruction(TACInstType::CALL){};

    TACCall(std::optional<TACValue> dest, std::string functionName, std::vector<TACValue> args) : TACInstruction(TACInstType::CALL), dest(dest), functionName(functionName), args(args){};
};

class TACReturn : public TACInstruction
{
public:
    std::optional<TACValue> ReturnValue = std::nullopt;

    TACReturn() : TACInstruction(TACInstType::RETURN){};

    TACReturn(std::optional<TACValue> returnValue) : TACInstruction(TACInstType::RETURN), ReturnValue(returnValue){};
};

inline std::unique_ptr<TACInstruction> cloneTACInstruction(TACInstruction* inst)
{
    static_assert(std::to_underlying(TACInstType::COUNT) == 9, "Add another case and increment the count check when adding a new TACInstType!!!");

    switch (inst->type)
    {
        case TACInstType::ASSIGN:
            return std::make_unique<TACAssign>(*static_cast<TACAssign*>(inst));
        case TACInstType::JUMP:
            return std::make_unique<TACJump>(*static_cast<TACJump*>(inst));
        case TACInstType::BINARYOP:
            return std::make_unique<TACBinaryOp>(*static_cast<TACBinaryOp*>(inst));
        case TACInstType::RETURN:
            return std::make_unique<TACReturn>(*static_cast<TACReturn*>(inst));
        case TACInstType::BRANCH:
            return std::make_unique<TACBranch>(*static_cast<TACBranch*>(inst));
        case TACInstType::CALL:
            return std::make_unique<TACCall>(*static_cast<TACCall*>(inst));
        case TACInstType::NEG:
            return std::make_unique<TACNeg>(*static_cast<TACNeg*>(inst));
        case TACInstType::PHI:
            return std::make_unique<TACPhi>(*static_cast<TACPhi*>(inst));
        case TACInstType::SELECT:
            return std::make_unique<TACSelect>(*static_cast<TACSelect*>(inst));
    }

    return nullptr;
}

struct TACInstOperands
{
    std::unordered_set<TACValue*> Operands;

    TACValue* Def = nullptr;
    std::unordered_set<TACValue*> Uses;

    TACInstOperands(){};

    TACInstOperands(TACValue* def, std::unordered_set<TACValue*> uses) : Def(def), Uses(uses)
    {
        if (Def != nullptr)
            Operands.insert(Def);

        Operands.insert(Uses.begin(), Uses.end());
    };
};

inline TACInstOperands GetTACInstOperands(TACInstruction* inst)
{
    static_assert(std::to_underlying(TACInstType::COUNT) == 9, "Add another case and increment the count check when adding a new TACInstType!!!");

    switch (inst->type)
    {
        case TACInstType::ASSIGN:
            {
                TACAssign* assign = static_cast<TACAssign*>(inst);
                return {&assign->dest, {&assign->source}};
            }
        case TACInstType::BINARYOP:
            {
                TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst);
                return {&binary->dest, {&binary->left, &binary->right}};
            }
        case TACInstType::RETURN:
            {
                TACReturn* ret = static_cast<TACReturn*>(inst);
                if (ret->ReturnValue.has_value())
                    return {nullptr, {&ret->ReturnValue.value()}};
                else
                    return {};
            }
        case TACInstType::BRANCH:
            {
                TACBranch* branch = static_cast<TACBranch*>(inst);
                return {nullptr, {&branch->cond.Left, &branch->cond.Right}};
            }
        case TACInstType::CALL:
            {
                TACCall* call = static_cast<TACCall*>(inst);

                std::unordered_set<TACValue*> pointerArgs;

                for (TACValue& val : call->args)
                {
                    pointerArgs.insert(&val);
                }

                if (call->dest.has_value())
                    return {&call->dest.value(), pointerArgs};
                else
                    return {nullptr, pointerArgs};
            }
        case TACInstType::NEG:
            {
                TACNeg* neg = static_cast<TACNeg*>(inst);
                return {&neg->dest, {&neg->source}};
            }
        case TACInstType::PHI:
            {
                TACPhi* phi = static_cast<TACPhi*>(inst);

                std::unordered_set<TACValue*> pointerArgs;

                for (PhiArgument& arg : phi->args)
                {
                    pointerArgs.insert(&arg.Value);
                }

                return {&phi->variable, pointerArgs};
            }
        case TACInstType::SELECT:
            {
                TACSelect* select = static_cast<TACSelect*>(inst);
                return {&select->dest, {&select->cond.Left, &select->cond.Right}};
            }

        case TACInstType::JUMP:
            return {};
    }

    return {};
}
