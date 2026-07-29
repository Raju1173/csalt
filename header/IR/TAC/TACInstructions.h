#pragma once

#include "IRCommon.h"
#include <optional>
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

inline std::string BinaryOpToStr[] = {
    "+",
    "-",
    "*",
    "/",

    "==",
    "!=",

    "<",
    "<=",

    ">",
    ">=",
};

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

class TACBlock;

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

    TACBinaryOp() : TACInstruction(TACType::BINARYOP){};

    TACBinaryOp(TACValue dest, TACValue left, BinaryOp op, TACValue right) : TACInstruction(TACType::BINARYOP), dest(dest), left(left), op(op), right(right){};
};

class TACNeg : public TACInstruction
{
public:
    TACValue dest;
    TACValue source;

    TACNeg() : TACInstruction(TACType::NEG){};

    TACNeg(TACValue dest, TACValue source) : TACInstruction(TACType::NEG), dest(dest), source(source){};
};

class TACAssign : public TACInstruction
{
public:
    TACValue dest;
    TACValue source;

    TACAssign() : TACInstruction(TACType::ASSIGN){};

    TACAssign(TACValue dest, TACValue source) : TACInstruction(TACType::ASSIGN), dest(dest), source(source){};
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

    TACPhi(TACValue var, std::vector<PhiArgument> args) : TACInstruction(TACType::PHI), variable(var), args(args){};
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

    TACBranch(Comparison cond, TACBlock* trueTarget, TACBlock* falseTarget) : TACInstruction(TACType::BRANCH), cond(cond), TrueTarget(trueTarget), FalseTarget(falseTarget){};
};

class TACSelect : public TACInstruction
{
public:
    TACValue dest;
    Comparison cond;
    TACValue TrueVal;
    TACValue FalseVal;

    TACSelect() : TACInstruction(TACType::SELECT){};

    TACSelect(TACValue dest, Comparison cond, TACValue trueVal, TACValue falseVal) : TACInstruction(TACType::SELECT), dest(dest), cond(cond), TrueVal(trueVal), FalseVal(falseVal){};
};

class TACCall : public TACInstruction
{
public:
    std::optional<TACValue> dest;
    std::string functionName;
    std::vector<TACValue> args;

    TACCall() : TACInstruction(TACType::CALL){};

    TACCall(std::optional<TACValue> dest, std::string functionName, std::vector<TACValue> args) : TACInstruction(TACType::CALL), dest(dest), functionName(functionName), args(args){};
};

class TACReturn : public TACInstruction
{
public:
    std::optional<TACValue> ReturnValue = std::nullopt;

    TACReturn() : TACInstruction(TACType::RETURN){};

    TACReturn(std::optional<TACValue> returnValue) : TACInstruction(TACType::RETURN), ReturnValue(returnValue){};
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
