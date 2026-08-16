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
    R10B,
    R11D,

    EBX,
    R12D,
    R13D,
    R14D,
    R15D,

    RSP,
    RBP,

    COUNT
};

inline std::string RegisterToStr(Register reg)
{
    static_assert(std::to_underlying(Register::COUNT) == 17, "Add/Remove the relevant case from the switch when modifying the Register enum!!!");

    switch (reg)
    {
        case Register::EAX:
            return "eax";

        case Register::EDI:
            return "edi";
        case Register::ESI:
            return "esi";
        case Register::EDX:
            return "edx";
        case Register::ECX:
            return "ecx";
        case Register::R8D:
            return "r8d";
        case Register::R9D:
            return "r9d";

        case Register::R10D:
            return "r10d";
        case Register::R10B:
            return "r10b";
        case Register::R11D:
            return "r11d";

        case Register::EBX:
            return "ebx";
        case Register::R12D:
            return "r12d";
        case Register::R13D:
            return "r13d";
        case Register::R14D:
            return "r14d";
        case Register::R15D:
            return "r15d";

        case Register::RSP:
            return "rsp";
        case Register::RBP:
            return "rbp";
    }
}

struct VirtualRegister
{
    int ID;
};

struct StackOffset
{
    int Offset;

    bool RSP = false;
};

struct Immediate
{
    int Value;
};

using Operand = std::variant<Register, VirtualRegister, StackOffset, Immediate>;

inline std::string OperandString(const Operand& op)
{
    if (std::holds_alternative<Register>(op))
    {
        return RegisterToStr(std::get<Register>(op));
    }

    else if (std::holds_alternative<VirtualRegister>(op))
    {
        return "vreg." + std::to_string(std::get<VirtualRegister>(op).ID);
    }

    else if (std::holds_alternative<StackOffset>(op))
    {
        std::string base = std::get<StackOffset>(op).RSP ? "rsp" : "rbp";

        if (std::get<StackOffset>(op).Offset < 0)
            return "DWORD PTR [" + base + std::to_string(std::get<StackOffset>(op).Offset) + "]";
        if (std::get<StackOffset>(op).Offset > 0)
            return "DWORD PTR [" + base + " + " + std::to_string(std::get<StackOffset>(op).Offset) + "]";

        return "DWORD PTR [" + base + "]";
    }

    else
    {
        return std::to_string(std::get<Immediate>(op).Value);
    }
}

enum class MIRInstType
{
    MOV,
    MOVZX,
    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    SHL,
    SAR,
    LEA,
    XOR,
    CMP,
    TEST,
    JMP,
    POP,
    CDQ,
    RET,
    PUSH,
    CJMP,
    CALL,
    SET,

    COUNT
};

class MIRBlock;
class MIRFunction;

class MIRInstruction
{
public:
    MIRInstType type;

    std::vector<Message> History;

    DeadSiblings<MIRInstruction> deadSiblings;

    MIRInstruction(MIRInstType type) : type(type){};

    virtual ~MIRInstruction() = default;
};

class MIRMov : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRMov() : MIRInstruction(MIRInstType::MOV){};
    MIRMov(Operand dest, Operand source) : MIRInstruction(MIRInstType::MOV), Dest(dest), Source(source){};
};

class MIRMovzx : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRMovzx() : MIRInstruction(MIRInstType::MOVZX){};
    MIRMovzx(Operand dest, Operand source) : MIRInstruction(MIRInstType::MOVZX), Dest(dest), Source(source){};
};

class MIRAdd : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRAdd() : MIRInstruction(MIRInstType::ADD){};
    MIRAdd(Operand dest, Operand source) : MIRInstruction(MIRInstType::ADD), Dest(dest), Source(source){};
};

class MIRSub : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRSub() : MIRInstruction(MIRInstType::SUB){};
    MIRSub(Operand dest, Operand source) : MIRInstruction(MIRInstType::SUB), Dest(dest), Source(source){};
};

class MIRImul : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRImul() : MIRInstruction(MIRInstType::MUL){};
    MIRImul(Operand dest, Operand source) : MIRInstruction(MIRInstType::MUL), Dest(dest), Source(source){};
};

class MIRIdiv : public MIRInstruction
{
public:
    Operand Divisor;

    MIRIdiv() : MIRInstruction(MIRInstType::DIV){};
    MIRIdiv(Operand divisor) : MIRInstruction(MIRInstType::DIV), Divisor(divisor){};
};

class MIRNeg : public MIRInstruction
{
public:
    Operand Dest;

    MIRNeg() : MIRInstruction(MIRInstType::NEG){};
    MIRNeg(Operand dest) : MIRInstruction(MIRInstType::NEG), Dest(dest){};
};

class MIRShl : public MIRInstruction
{
public:
    Operand Dest;
    Operand Count;

    MIRShl() : MIRInstruction(MIRInstType::SHL){};
    MIRShl(Operand dest, Operand count) : MIRInstruction(MIRInstType::SHL), Dest(dest), Count(count){};
};

class MIRSar : public MIRInstruction
{
public:
    Operand Dest;
    Operand Count;

    MIRSar() : MIRInstruction(MIRInstType::SAR){};
    MIRSar(Operand dest, Operand count) : MIRInstruction(MIRInstType::SAR), Dest(dest), Count(count){};
};

class MIRLea : public MIRInstruction
{
public:
    Operand Dest;
    Operand Base;
    Operand Index;
    Operand Scale;

    MIRLea() : MIRInstruction(MIRInstType::LEA){};
    MIRLea(Operand dest, Operand base, Operand index, Operand scale) : MIRInstruction(MIRInstType::LEA), Dest(dest), Base(base), Index(index), Scale(scale){};
};

class MIRXor : public MIRInstruction
{
public:
    Operand Dest;
    Operand Source;

    MIRXor() : MIRInstruction(MIRInstType::XOR){};
    MIRXor(Operand dest, Operand source) : MIRInstruction(MIRInstType::XOR), Dest(dest), Source(source){};
};

class MIRCmp : public MIRInstruction
{
public:
    Operand Left;
    Operand Right;

    MIRCmp() : MIRInstruction(MIRInstType::CMP){};
    MIRCmp(Operand left, Operand right) : MIRInstruction(MIRInstType::CMP), Left(left), Right(right){};
};

class MIRTest : public MIRInstruction
{
public:
    Operand Left;
    Operand Right;

    MIRTest() : MIRInstruction(MIRInstType::TEST){};
    MIRTest(Operand left, Operand right) : MIRInstruction(MIRInstType::TEST), Left(left), Right(right){};
};

class MIRJump : public MIRInstruction
{
public:
    MIRBlock* TargetBlock;

    MIRJump() : MIRInstruction(MIRInstType::JMP){};
    MIRJump(MIRBlock* targetBlock) : MIRInstruction(MIRInstType::JMP), TargetBlock(targetBlock){};
};

class MIRPush : public MIRInstruction
{
public:
    Operand Source;

    MIRPush() : MIRInstruction(MIRInstType::PUSH){};
    MIRPush(Operand source) : MIRInstruction(MIRInstType::PUSH), Source(source){};
};

class MIRPop : public MIRInstruction
{
public:
    Operand Dest;

    MIRPop() : MIRInstruction(MIRInstType::POP){};
    MIRPop(Operand dest) : MIRInstruction(MIRInstType::POP), Dest(dest){};
};

enum class Condition
{
    EQUAL,
    NOT_EQUAL,

    LESS,
    LESS_EQUAL,

    GREATER,
    GREATER_EQUAL,
};

class MIRCondJump : public MIRInstruction
{
public:
    Condition Cond;
    MIRBlock* TargetBlock;

    MIRCondJump() : MIRInstruction(MIRInstType::CJMP){};
    MIRCondJump(Condition cond, MIRBlock* targetBlock) : MIRInstruction(MIRInstType::CJMP), Cond(cond), TargetBlock(targetBlock){};
};

class MIRCall : public MIRInstruction
{
public:
    MIRFunction* Function;

    MIRCall() : MIRInstruction(MIRInstType::CALL){};
    MIRCall(MIRFunction* function) : MIRInstruction(MIRInstType::CALL), Function(function){};
};

class MIRSet : public MIRInstruction
{
public:
    Condition Cond;
    Operand Dest;

    MIRSet() : MIRInstruction(MIRInstType::SET){};
    MIRSet(Condition cond, Operand dest) : MIRInstruction(MIRInstType::SET), Cond(cond), Dest(dest){};
};

class MIRRet : public MIRInstruction
{
public:
    MIRRet() : MIRInstruction(MIRInstType::RET){};
};

class MIRCdq : public MIRInstruction
{
public:
    MIRCdq() : MIRInstruction(MIRInstType::CDQ){};
};

inline std::unique_ptr<MIRInstruction> cloneMIRInstruction(MIRInstruction* inst)
{
    static_assert(std::to_underlying(MIRInstType::COUNT) == 21, "Add another case and increment the count check when adding a new MIRInstType!!!");

    switch (inst->type)
    {
        case MIRInstType::MOV:
            return std::make_unique<MIRMov>(*static_cast<MIRMov*>(inst));
        case MIRInstType::MOVZX:
            return std::make_unique<MIRMovzx>(*static_cast<MIRMovzx*>(inst));
        case MIRInstType::ADD:
            return std::make_unique<MIRAdd>(*static_cast<MIRAdd*>(inst));
        case MIRInstType::SUB:
            return std::make_unique<MIRSub>(*static_cast<MIRSub*>(inst));
        case MIRInstType::MUL:
            return std::make_unique<MIRImul>(*static_cast<MIRImul*>(inst));
        case MIRInstType::DIV:
            return std::make_unique<MIRIdiv>(*static_cast<MIRIdiv*>(inst));
        case MIRInstType::NEG:
            return std::make_unique<MIRNeg>(*static_cast<MIRNeg*>(inst));
        case MIRInstType::SHL:
            return std::make_unique<MIRShl>(*static_cast<MIRShl*>(inst));
        case MIRInstType::SAR:
            return std::make_unique<MIRSar>(*static_cast<MIRSar*>(inst));
        case MIRInstType::LEA:
            return std::make_unique<MIRLea>(*static_cast<MIRLea*>(inst));
        case MIRInstType::XOR:
            return std::make_unique<MIRXor>(*static_cast<MIRXor*>(inst));
        case MIRInstType::CMP:
            return std::make_unique<MIRCmp>(*static_cast<MIRCmp*>(inst));
        case MIRInstType::TEST:
            return std::make_unique<MIRTest>(*static_cast<MIRTest*>(inst));
        case MIRInstType::JMP:
            return std::make_unique<MIRJump>(*static_cast<MIRJump*>(inst));
        case MIRInstType::POP:
            return std::make_unique<MIRPop>(*static_cast<MIRPop*>(inst));
        case MIRInstType::CDQ:
            return std::make_unique<MIRCdq>(*static_cast<MIRCdq*>(inst));
        case MIRInstType::RET:
            return std::make_unique<MIRRet>(*static_cast<MIRRet*>(inst));
        case MIRInstType::PUSH:
            return std::make_unique<MIRPush>(*static_cast<MIRPush*>(inst));
        case MIRInstType::CJMP:
            return std::make_unique<MIRCondJump>(*static_cast<MIRCondJump*>(inst));
        case MIRInstType::CALL:
            return std::make_unique<MIRCall>(*static_cast<MIRCall*>(inst));
        case MIRInstType::SET:
            return std::make_unique<MIRSet>(*static_cast<MIRSet*>(inst));
    }

    return nullptr;
}
