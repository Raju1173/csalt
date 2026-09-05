#include "IRFormatters.h"
#include "Globals.h"
#include "IRCommon.h"
#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include "TACGenerator.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <string>
#include <format>
#include <algorithm>
#include <variant>

std::string FormatTokens(TokenStream& tokenStream)
{
    std::string out;

    for (const Token& t : tokenStream)
    {
        if (t.type == TokenType::IDENTIFIER || t.type == TokenType::NUMBER)
            out += std::format("{}({})\n", TokenTypeToStr(t.type), t.lexeme);
        else
            out += std::format("{}\n", TokenTypeToStr(t.type));
    }

    out += "\n\n";

    return out;
}

std::string FormatNode(Node& node, int depth = 0)
{
    std::string out;

    for (int i = 0; i < depth; i++)
        out += "|    ";

    if (node.type == NodeType::NUMBER || node.type == NodeType::IDENTIFIER || node.type == NodeType::CALL || node.type == NodeType::BINARY_OP || node.type == NodeType::VAR || node.type == NodeType::FUNCTION || node.type == NodeType::UNARY_OP)
    {
        out += std::format("{} ({})\n", NodeTypeToStr(node.type), (node.type == NodeType::BINARY_OP || node.type == NodeType::UNARY_OP) ? TokenTypeToStr(node.token.type) : node.token.lexeme);
    }

    else
    {
        out += std::format("{}\n", NodeTypeToStr(node.type));
    }

    for (size_t i = 0; i < node.children.size(); i++)
        out += FormatNode(node.children[i], depth + 1);

    return out;
}

std::string FormatAST(Node& root)
{
    std::string out;

    out += FormatNode(root, 0);
    out += "\n";

    return out;
}

std::string FormatCFGBlock(CFGBlock* Block)
{
    std::string out;

    out += "|    Statements :\n";

    for (size_t i = 0; i < Block->Statements.size(); i++)
    {
        out += FormatNode(Block->Statements[i], 2);
    }

    if (Block->Condition.has_value())
    {
        out += "\n|    Condition :\n";
        out += FormatNode(*(Block->Condition), 2);
    }

    if (Block->TransitionNext.has_value())
        out += std::format("\n|    Transition Next : Block - {}\n", Block->TransitionNext.value()->ID);

    if (Block->TransitionTrue.has_value())
        out += std::format("\n|    Transition True : Block - {}\n", Block->TransitionTrue.value()->ID);

    if (Block->TransitionFalse.has_value())
        out += std::format("\n|    Transition False : Block - {}\n", Block->TransitionFalse.value()->ID);

    return out;
}

std::string FormatCFG(CFG& CFG)
{
    std::string out;

    for (size_t i = 0; i < CFG.size(); i++)
    {
        out += "\n# Function(";

        for (size_t j = 0; j < CFG[i].Parameters.size(); ++j)
        {
            if (j != 0)
                out += ", ";

            out += CFG[i].Parameters[j];
        }

        out += std::format(") - {} :\n", CFG[i].FunctionName);

        for (size_t j = 0; j < CFG[i].Blocks.size(); j++)
        {
            out += std::format("\nBlock - {} :\n\n", CFG[i].Blocks[j]->ID);

            out += FormatCFGBlock(CFG[i].Blocks[j].get());
        }
    }

    out += "\n";

    return out;
}

std::string FormatMessage(Message& msg)
{
    std::string color = "\033[1;30m";

    switch (msg.TranformationType)
    {
        case IRTransformType::DELETED:
            color = "\033[0;91m";
            break;

        case IRTransformType::ADDED:
        case IRTransformType::CLONED:
            color = "\033[0;92m";
            break;

        case IRTransformType::RENAMED:
        case IRTransformType::REPLACED:
            color = "\033[0;93m";
            break;

        case IRTransformType::MOVED:
            color = "\033[0;94m";
            break;
    }

    std::string out;

    // └ = U+2514
    // ─ = U+2500
    out += std::format("\033[0m      └─{}{:<11}\033[0m BY \033[0;96m{:<33}\033[0m : {}\n", color, "[" + IRTransformTypeToStr(msg.TranformationType) + "]", "[" + getPhaseMetadata(msg.Pass).name + "]", msg.Info);

    return out;
}

std::string FormatTACInstruction(TACInstruction* inst, bool history)
{
    std::string out;

    switch (inst->type)
    {
        case TACInstType::PHI:
            {
                TACPhi* phi = static_cast<TACPhi*>(inst);

                out += std::format("    {} = PHI(", TACValToStr(phi->variable));

                for (size_t i = 0; i < phi->args.size(); i++)
                {
                    out += std::format("{} from B{}", TACValToStr(phi->args[i].Value), phi->args[i].SourceBlock->ID);

                    if (i + 1 != phi->args.size())
                        out += ", ";
                }

                out += ")\n";
            }
            break;

        case TACInstType::ASSIGN:
            {
                TACAssign* assign = static_cast<TACAssign*>(inst);

                out += std::format("    {} = {}\n", TACValToStr(assign->dest), TACValToStr(assign->source));
            }
            break;

        case TACInstType::NEG:
            {
                TACNeg* neg = static_cast<TACNeg*>(inst);

                out += std::format("    {} = neg {}\n", TACValToStr(neg->dest), TACValToStr(neg->source));
            }
            break;

        case TACInstType::BINARYOP:
            {
                TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst);

                out += std::format("    {} = {} {} {}\n", TACValToStr(binary->dest), TACValToStr(binary->left), BinaryOpToStr(binary->op), TACValToStr(binary->right));
            }
            break;

        case TACInstType::CALL:
            {
                TACCall* call = static_cast<TACCall*>(inst);

                if (call->dest.has_value())
                    out += std::format("    {} = ", TACValToStr(call->dest.value()));
                else
                    out += "    ";

                out += std::format("{}call {}(", (call->isSiblingCall ? "sibling_" : ""), call->functionName);

                for (size_t i = 0; i < call->args.size(); i++)
                {
                    out += TACValToStr(call->args[i]);

                    if (i + 1 != call->args.size())
                        out += ", ";
                }

                out += ")\n";
            }
            break;

        case TACInstType::BRANCH:
            {
                TACBranch* branch = static_cast<TACBranch*>(inst);

                out += std::format("    branch ({} {} {}) ? B{} : B{}\n", TACValToStr(branch->cond.Left), BinaryOpToStr(branch->cond.Op), TACValToStr(branch->cond.Right), branch->TrueTarget->ID, branch->FalseTarget->ID);
            }
            break;

        case TACInstType::SELECT:
            {
                TACSelect* select = static_cast<TACSelect*>(inst);

                out += std::format("    {} = select ({} {} {}) ? {} : {}\n", TACValToStr(select->dest), TACValToStr(select->cond.Left), BinaryOpToStr(select->cond.Op), TACValToStr(select->cond.Right), TACValToStr(select->TrueVal), TACValToStr(select->FalseVal));
            }
            break;

        case TACInstType::JUMP:
            {
                TACJump* jump = static_cast<TACJump*>(inst);

                out += std::format("    jump B{}\n", jump->TargetBlock->ID);
            }
            break;

        case TACInstType::RETURN:
            {
                TACReturn* ret = static_cast<TACReturn*>(inst);

                if (!ret->ReturnValue.has_value())
                    out += "    return\n";
                else
                    out += std::format("    return {}\n", TACValToStr(ret->ReturnValue.value()));
            }
            break;
    }

    out += "\033[0m";

    if (history)
    {
        for (Message& message : inst->History)
        {
            out += FormatMessage(message);
        }
    }

    return out;
}

std::string FormatTACBlock(TACBlock* block, bool history)
{
    std::string out;

    auto blockDead = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    out += std::format("\nBlock - {} :\n\033[0m", block->ID);

    if (history)
    {
        for (Message& message : block->History)
        {
            out += FormatMessage(message);
        }
    }

    if (history)
    {
        if (block->LastDeadInstruction != nullptr)
        {
            for (auto& prec : block->LastDeadInstruction->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatTACInstruction(prec.get(), history);
            }

            out += "\033[2;37m";
            out += FormatTACInstruction(block->LastDeadInstruction.get(), history);

            for (auto& trail : block->LastDeadInstruction->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatTACInstruction(trail.get(), history);
            }
        }
    }

    for (auto& inst : block->Instructions)
    {
        if (history)
        {
            for (auto& prec : inst->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatTACInstruction(prec.get(), history);
            }
        }

        if (blockDead == block->Function->Blocks.end())
            out += "\033[2;37m";

        out += FormatTACInstruction(inst.get(), history);

        if (history)
        {
            for (auto& trail : inst->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatTACInstruction(trail.get(), history);
            }
        }
    }

    return out;
}

std::string FormatTACFunction(TACFunction* func, bool history)
{
    std::string out;

    out += std::format("# Function - {}(", func->Name);

    for (size_t j = 0; j < func->Parameters.size(); ++j)
    {
        if (j != 0)
            out += ", ";

        out += func->Parameters[j];
    }

    out += ") :\n\033[0m";

    if (history)
    {
        for (Message& message : func->History)
        {
            out += FormatMessage(message);
        }
    }

    if (history)
    {
        if (func->LastDeadBlock != nullptr)
        {
            for (auto& prec : func->LastDeadBlock->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatTACBlock(prec.get(), history);
            }

            out += "\033[2;37m";
            out += FormatTACBlock(func->LastDeadBlock.get(), history);

            for (auto& trail : func->LastDeadBlock->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatTACBlock(trail.get(), history);
            }

            out += std::format("\n\033[2;37mend {}\033[0m\n\n", func->Name);

            return out;
        }
    }

    for (auto& block : func->Blocks)
    {
        if (history)
        {
            for (auto& prec : block->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatTACBlock(prec.get(), history);
            }
        }

        out += FormatTACBlock(block.get(), history);

        if (history)
        {
            for (auto& trail : block->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatTACBlock(trail.get(), history);
            }
        }
    }

    out += std::format("\nend {}\n\n", func->Name);

    return out;
}

std::string FormatTAC(TAC& TAC, bool history)
{
    std::string out;

    for (auto& func : TAC)
    {
        if (history)
        {
            for (auto& prec : func->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatTACFunction(prec.get(), history);
            }
        }

        out += FormatTACFunction(func.get(), history);

        if (history)
        {
            for (auto& trail : func->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatTACFunction(trail.get(), history);
            }
        }
    }

    out += "\n";

    return out;
}

std::string FormatMIRInstruction(MIRFunction* function, MIRInstruction* inst, bool history)
{
    std::string out;

    switch (inst->type)
    {
        case MIRInstType::MOV:
            {
                MIRMov* mov = static_cast<MIRMov*>(inst);

                out += std::format("    mov {}, {}\n", OperandString(mov->Dest), OperandString(mov->Source));
            }
            break;

        case MIRInstType::MOVZX:
            {
                MIRMovzx* movzx = static_cast<MIRMovzx*>(inst);

                out += std::format("    movzx {}, {}\n", OperandString(movzx->Dest), OperandString(movzx->Source));
            }
            break;

        case MIRInstType::CMOV:
            {
                MIRCmov* cmov = static_cast<MIRCmov*>(inst);

                out += "    ";

                switch (cmov->Cond)
                {
                    case Condition::EQUAL:
                        out += "cmove ";
                        break;
                    case Condition::NOT_EQUAL:
                        out += "cmovne ";
                        break;
                    case Condition::LESS:
                        out += "cmovl ";
                        break;
                    case Condition::LESS_EQUAL:
                        out += "cmovle ";
                        break;
                    case Condition::GREATER:
                        out += "cmovg ";
                        break;
                    case Condition::GREATER_EQUAL:
                        out += "cmovge ";
                        break;
                }

                out += std::format("{}, {}\n", OperandString(cmov->Dest), OperandString(cmov->Source));
            }
            break;

        case MIRInstType::ADD:
            {
                MIRAdd* add = static_cast<MIRAdd*>(inst);

                out += std::format("    add {}, {}\n", OperandString(add->Dest), OperandString(add->Source));
            }
            break;

        case MIRInstType::SUB:
            {
                MIRSub* sub = static_cast<MIRSub*>(inst);

                out += std::format("    sub {}, {}\n", OperandString(sub->Dest), OperandString(sub->Source));
            }
            break;

        case MIRInstType::MUL:
            {
                MIRImul* mul = static_cast<MIRImul*>(inst);

                out += std::format("    imul {}, {}\n", OperandString(mul->Dest), OperandString(mul->Source));
            }
            break;

        case MIRInstType::DIV:
            {
                MIRIdiv* div = static_cast<MIRIdiv*>(inst);

                out += std::format("    idiv {}\n", OperandString(div->Divisor));
            }
            break;

        case MIRInstType::NEG:
            {
                MIRNeg* neg = static_cast<MIRNeg*>(inst);

                out += std::format("    neg {}\n", OperandString(neg->Dest));
            }
            break;

        case MIRInstType::XOR:
            {
                MIRXor* xorInst = static_cast<MIRXor*>(inst);

                out += std::format("    xor {}, {}\n", OperandString(xorInst->Dest), OperandString(xorInst->Source));
            }
            break;

        case MIRInstType::CMP:
            {
                MIRCmp* cmp = static_cast<MIRCmp*>(inst);

                out += std::format("    cmp {}, {}\n", OperandString(cmp->Left), OperandString(cmp->Right));
            }
            break;

        case MIRInstType::TEST:
            {
                MIRTest* test = static_cast<MIRTest*>(inst);

                out += std::format("    test {}, {}\n", OperandString(test->Left), OperandString(test->Right));
            }
            break;

        case MIRInstType::PUSH:
            {
                MIRPush* push = static_cast<MIRPush*>(inst);

                out += std::format("    push {}\n", OperandString(push->Source));
            }
            break;

        case MIRInstType::POP:
            {
                MIRPop* pop = static_cast<MIRPop*>(inst);

                out += std::format("    pop {}\n", OperandString(pop->Dest));
            }
            break;

        case MIRInstType::JMP:
            {
                MIRJump* jump = static_cast<MIRJump*>(inst);

                if (std::holds_alternative<MIRBlock*>(jump->Target))
                    out += std::format("    jmp B{}\n", std::get<MIRBlock*>(jump->Target)->ID);

                if (std::holds_alternative<MIRFunction*>(jump->Target))
                    out += std::format("    jmp {}\n", std::get<MIRFunction*>(jump->Target)->FunctionName);
            }
            break;

        case MIRInstType::CJMP:
            {
                MIRCondJump* jump = static_cast<MIRCondJump*>(inst);

                out += "    ";

                switch (jump->Cond)
                {
                    case Condition::EQUAL:
                        out += "je ";
                        break;
                    case Condition::NOT_EQUAL:
                        out += "jne ";
                        break;
                    case Condition::LESS:
                        out += "jl ";
                        break;
                    case Condition::LESS_EQUAL:
                        out += "jle ";
                        break;
                    case Condition::GREATER:
                        out += "jg ";
                        break;
                    case Condition::GREATER_EQUAL:
                        out += "jge ";
                        break;
                }

                out += std::format("B{}\n", jump->TargetBlock->ID);
            }
            break;

        case MIRInstType::CALL:
            {
                MIRCall* call = static_cast<MIRCall*>(inst);

                out += std::format("    call {}\n", call->Function->FunctionName);
            }
            break;

        case MIRInstType::SET:
            {
                MIRSet* set = static_cast<MIRSet*>(inst);

                out += "    ";

                switch (set->Cond)
                {
                    case Condition::EQUAL:
                        out += "sete ";
                        break;
                    case Condition::NOT_EQUAL:
                        out += "setne ";
                        break;
                    case Condition::LESS:
                        out += "setl ";
                        break;
                    case Condition::LESS_EQUAL:
                        out += "setle ";
                        break;
                    case Condition::GREATER:
                        out += "setg ";
                        break;
                    case Condition::GREATER_EQUAL:
                        out += "setge ";
                        break;
                }

                out += std::format("{}\n", OperandString(set->Dest));
            }
            break;

        case MIRInstType::RET:
            {
                out += "    ret\n";
            }
            break;

        case MIRInstType::CDQ:
            {
                out += "    cdq\n";
            }
            break;
    }

    out += "\033[0m";

    if (history)
    {
        for (Message& message : inst->History)
        {
            out += FormatMessage(message);
        }
    }

    return out;
}

std::string FormatMIRBlock(MIRBlock* block, bool history)
{
    std::string out;

    auto blockDead = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    out += std::format("Block - {} :\n\033[0m", block->ID);

    if (history)
    {
        for (Message& message : block->History)
        {
            out += FormatMessage(message);
        }
    }

    if (history)
    {
        if (block->LastDeadInstruction != nullptr)
        {
            for (auto& prec : block->LastDeadInstruction->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatMIRInstruction(block->Function, prec.get(), history);
            }

            out += "\033[2;37m";
            out += FormatMIRInstruction(block->Function, block->LastDeadInstruction.get(), history);

            for (auto& trail : block->LastDeadInstruction->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatMIRInstruction(block->Function, trail.get(), history);
            }
        }
    }

    for (auto& inst : block->Instructions)
    {
        if (history)
        {
            for (auto& prec : inst->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatMIRInstruction(block->Function, prec.get(), history);
            }
        }

        if (blockDead == block->Function->Blocks.end())
            out += "\033[2;37m";

        out += FormatMIRInstruction(block->Function, inst.get(), history);

        if (history)
        {
            for (auto& trail : inst->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatMIRInstruction(block->Function, trail.get(), history);
            }
        }
    }

    out += "\n";

    return out;
}

std::string FormatMIRFunction(MIRFunction* func, bool history)
{
    std::string out;

    out += std::format("# Function - {}(", func->FunctionName);

    for (size_t j = 0; j < func->Parameters.size(); ++j)
    {
        if (j != 0)
            out += ", ";

        out += func->Parameters[j];
    }

    out += ") :\n\n\033[0m";

    if (history)
    {
        for (Message& message : func->History)
        {
            out += FormatMessage(message);
        }
    }

    if (history)
    {
        if (func->LastDeadBlock != nullptr)
        {
            for (auto& prec : func->LastDeadBlock->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatMIRBlock(prec.get(), history);
            }

            out += "\033[2;37m";
            out += FormatMIRBlock(func->LastDeadBlock.get(), history);

            for (auto& trail : func->LastDeadBlock->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatMIRBlock(trail.get(), history);
            }

            out += std::format("\n\033[2;37mend {}\033[0m\n\n", func->FunctionName);

            return out;
        }
    }

    for (auto& block : func->Blocks)
    {
        if (history)
        {
            for (auto& prec : block->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatMIRBlock(prec.get(), history);
            }
        }

        out += FormatMIRBlock(block.get(), history);

        if (history)
        {
            for (auto& trail : block->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatMIRBlock(trail.get(), history);
            }
        }
    }

    return out;
}

std::string FormatMIR(MIR& MIR, bool history)
{
    std::string out;

    for (auto& func : MIR)
    {
        if (history)
        {
            for (auto& prec : func->deadSiblings.preceding)
            {
                out += "\033[2;37m";
                out += FormatMIRFunction(prec.get(), history);
            }
        }

        out += FormatMIRFunction(func.get(), history);

        if (history)
        {
            for (auto& trail : func->deadSiblings.trailing)
            {
                out += "\033[2;37m";
                out += FormatMIRFunction(trail.get(), history);
            }
        }
    }

    out += "\n";

    return out;
}

std::string GetASM(std::string AsmFilePath)
{
    std::ifstream AsmFile(AsmFilePath, std::ios::in | std::ios::binary | std::ios::ate);

    std::streamsize size = AsmFile.tellg();

    AsmFile.seekg(0, std::ios::beg);

    std::string AsmOutput(size, '\0');

    AsmFile.read(AsmOutput.data(), size);

    return AsmOutput;
}
