#include "TACGenerator.h"
#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <cstddef>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <print>
#include <ranges>

size_t NextTemp = 0;

BinaryOp TokenOpToBinaryOp(TokenType op)
{
    switch (op)
    {
        case TokenType::PLUS:
            return BinaryOp::PLUS;

        case TokenType::MINUS:
            return BinaryOp::MINUS;

        case TokenType::ASTERISK:
            return BinaryOp::MUL;

        case TokenType::SLASH:
            return BinaryOp::DIV;

        case TokenType::DOUBLE_EQUAL:
            return BinaryOp::DOUBLE_EQUAL;

        case TokenType::NOT_EQUAL:
            return BinaryOp::NOT_EQUAL;

        case TokenType::LESS:
            return BinaryOp::LESS;

        case TokenType::LESS_EQUAL:
            return BinaryOp::LESS_EQUAL;

        case TokenType::GREATER:
            return BinaryOp::GREATER;

        case TokenType::GREATER_EQUAL:
            return BinaryOp::GREATER_EQUAL;
    }
}

TACValue GetTemp()
{
    return TACVariable{"t." + std::to_string(NextTemp++)};
}

TACValue flattenBinaryOpNode(Node& BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions);
TACValue flattenUnaryOpNode(Node& BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions);

std::optional<TACValue> flattenCallNode(Node& CallNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions, bool hasValue = true)
{
    auto call = std::make_unique<TACCall>();

    call->functionName = CallNode.token.lexeme;

    for (Node& child : CallNode.children)
    {
        if (child.children[0].type == NodeType::BINARY_OP)
            call->args.push_back(flattenBinaryOpNode(child.children[0], Instructions));

        else if (child.children[0].type == NodeType::UNARY_OP)
            call->args.push_back(flattenUnaryOpNode(child.children[0], Instructions));

        else if (child.children[0].type == NodeType::CALL)
            call->args.push_back(flattenCallNode(child.children[0], Instructions, true).value());

        else
        {
            if (child.children[0].token.type == TokenType::NUMBER)
                call->args.push_back(std::stoi(child.children[0].token.lexeme));
            else
                call->args.push_back(TACVariable{child.children[0].token.lexeme});
        }
    }

    std::optional<TACValue> returnVal = hasValue ? std::optional<TACValue>{GetTemp()} : std::nullopt;

    call->dest = returnVal;

    Instructions.push_back(std::move(call));

    return returnVal;
}

TACValue flattenBinaryOpNode(Node& BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions)
{
    auto binaryOp = std::make_unique<TACBinaryOp>();

    if (BinaryOpNode.children[0].type == NodeType::BINARY_OP)
        binaryOp->left = flattenBinaryOpNode(BinaryOpNode.children[0], Instructions);

    else if (BinaryOpNode.children[0].type == NodeType::UNARY_OP)
        binaryOp->left = flattenUnaryOpNode(BinaryOpNode.children[0], Instructions);

    else if (BinaryOpNode.children[0].type == NodeType::CALL)
        binaryOp->left = flattenCallNode(BinaryOpNode.children[0], Instructions, true).value();

    else
    {
        if (BinaryOpNode.children[0].token.type == TokenType::NUMBER)
            binaryOp->left = std::stoi(BinaryOpNode.children[0].token.lexeme);
        else
            binaryOp->left = TACVariable{BinaryOpNode.children[0].token.lexeme};
    }

    binaryOp->op = TokenOpToBinaryOp(BinaryOpNode.token.type);

    if (BinaryOpNode.children[1].type == NodeType::BINARY_OP)
        binaryOp->right = flattenBinaryOpNode(BinaryOpNode.children[1], Instructions);

    else if (BinaryOpNode.children[1].type == NodeType::UNARY_OP)
        binaryOp->right = flattenUnaryOpNode(BinaryOpNode.children[1], Instructions);

    else if (BinaryOpNode.children[1].type == NodeType::CALL)
        binaryOp->right = flattenCallNode(BinaryOpNode.children[1], Instructions).value();

    else
    {
        if (BinaryOpNode.children[1].token.type == TokenType::NUMBER)
            binaryOp->right = std::stoi(BinaryOpNode.children[1].token.lexeme);
        else
            binaryOp->right = TACVariable{BinaryOpNode.children[1].token.lexeme};
    }

    TACValue returnVal = GetTemp();

    binaryOp->dest = returnVal;

    Instructions.push_back(std::move(binaryOp));

    return returnVal;
}

TACValue flattenUnaryOpNode(Node& UnaryOpNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions)
{
    auto neg = std::make_unique<TACNeg>();

    if (UnaryOpNode.children[0].type == NodeType::BINARY_OP)
        neg->source = flattenBinaryOpNode(UnaryOpNode.children[0], Instructions);

    else if (UnaryOpNode.children[0].type == NodeType::UNARY_OP)
        neg->source = flattenUnaryOpNode(UnaryOpNode.children[0], Instructions);

    else if (UnaryOpNode.children[0].type == NodeType::CALL)
        neg->source = flattenCallNode(UnaryOpNode.children[0], Instructions, true).value();

    else
    {
        if (UnaryOpNode.children[0].token.type == TokenType::NUMBER)
            neg->source = stoi(UnaryOpNode.children[0].token.lexeme);
        else
            neg->source = TACVariable{UnaryOpNode.children[0].token.lexeme};
    }

    TACValue returnVal = GetTemp();

    neg->dest = returnVal;

    Instructions.push_back(std::move(neg));

    return returnVal;
}

TAC GenerateTAC(CFG& CFG)
{
    TAC TAC;

    for (CFGFunction& CFGFunc : CFG)
    {
        TAC.push_back(std::make_unique<TACFunction>(CFGFunc.FunctionName, CFGFunc.Parameters));

        std::unordered_map<CFGBlock*, TACBlock*> blockMap;

        for (auto& CFGBlock : std::views::reverse(CFGFunc.Blocks))
        {
            TAC.back()->Blocks.push_back(std::make_unique<TACBlock>(CFGBlock->ID, TAC.back().get()));
            blockMap[CFGBlock.get()] = TAC.back()->Blocks.back().get();
        }

        for (auto& CFGBlock : CFGFunc.Blocks)
        {
            TACBlock* curBlock = blockMap[CFGBlock.get()];

            for (Node& statement : CFGBlock->Statements)
            {
                switch (statement.type)
                {
                    case NodeType::VAR:
                        {
                            auto assign = std::make_unique<TACAssign>();

                            assign->dest = TACVariable{statement.token.lexeme};

                            if (statement.children.size() != 0)
                            {
                                if (statement.children[0].children[0].type == NodeType::BINARY_OP)
                                    assign->source = flattenBinaryOpNode(statement.children[0].children[0], curBlock->Instructions);

                                else if (statement.children[0].children[0].type == NodeType::UNARY_OP)
                                    assign->source = flattenUnaryOpNode(statement.children[0].children[0], curBlock->Instructions);

                                else if (statement.children[0].children[0].type == NodeType::CALL)
                                    assign->source = flattenCallNode(statement.children[0].children[0], curBlock->Instructions).value();

                                else
                                {
                                    if (statement.children[0].children[0].token.type == TokenType::NUMBER)
                                        assign->source = stoi(statement.children[0].children[0].token.lexeme);
                                    else
                                        assign->source = TACVariable{statement.children[0].children[0].token.lexeme};
                                }
                            }

                            else
                            {
                                assign->source = 0;
                            }

                            curBlock->Instructions.push_back(std::move(assign));
                        }
                        break;

                    case NodeType::EXPR:
                        {
                            auto assign = std::make_unique<TACAssign>();

                            assign->dest = TACVariable{statement.children[0].children[0].token.lexeme};

                            if (statement.children[0].children[1].type == NodeType::BINARY_OP)
                                assign->source = flattenBinaryOpNode(statement.children[0].children[1], curBlock->Instructions);

                            else if (statement.children[0].children[1].type == NodeType::UNARY_OP)
                                assign->source = flattenUnaryOpNode(statement.children[0].children[1], curBlock->Instructions);

                            else if (statement.children[0].children[1].type == NodeType::CALL)
                                assign->source = flattenCallNode(statement.children[0].children[1], curBlock->Instructions).value();

                            else
                            {
                                if (statement.children[0].children[1].token.type == TokenType::NUMBER)
                                    assign->source = stoi(statement.children[0].children[1].token.lexeme);
                                else
                                    assign->source = TACVariable{statement.children[0].children[1].token.lexeme};
                            }

                            curBlock->Instructions.push_back(std::move(assign));
                        }
                        break;

                    case NodeType::CALL:
                        flattenCallNode(statement, curBlock->Instructions, false);
                        break;

                    case NodeType::RETURN:
                        {
                            auto ret = std::make_unique<TACReturn>();

                            if (statement.children.size() != 0)
                            {
                                if (statement.children[0].children[0].type == NodeType::BINARY_OP)
                                    ret->ReturnValue = flattenBinaryOpNode(statement.children[0].children[0], curBlock->Instructions);

                                else if (statement.children[0].children[0].type == NodeType::UNARY_OP)
                                    ret->ReturnValue = flattenUnaryOpNode(statement.children[0].children[0], curBlock->Instructions);

                                else if (statement.children[0].children[0].type == NodeType::CALL)
                                    ret->ReturnValue = flattenCallNode(statement.children[0].children[0], curBlock->Instructions).value();

                                else
                                {
                                    if (statement.children[0].children[0].token.type == TokenType::NUMBER)
                                        ret->ReturnValue = std::stoi(statement.children[0].children[0].token.lexeme);
                                    else
                                        ret->ReturnValue = TACVariable{statement.children[0].children[0].token.lexeme};
                                }
                            }

                            else
                                ret->ReturnValue = std::nullopt;

                            curBlock->Instructions.push_back(std::move(ret));
                        }
                        break;
                }
            }

            if (CFGBlock->Condition.has_value())
            {
                flattenBinaryOpNode(CFGBlock->Condition->children[0], curBlock->Instructions);

                TACBinaryOp* TACBinOp = static_cast<TACBinaryOp*>(curBlock->Instructions.back().get());

                auto branch = std::make_unique<TACBranch>(Comparison{TACBinOp->left, TACBinOp->op, TACBinOp->right});

                curBlock->Instructions.pop_back();

                branch->TrueTarget = blockMap[CFGBlock->TransitionTrue.value()];
                branch->FalseTarget = blockMap[CFGBlock->TransitionFalse.value()];

                curBlock->Children.push_back(branch->TrueTarget);
                curBlock->Children.push_back(branch->FalseTarget);

                branch->TrueTarget->Parents.push_back(curBlock);
                branch->FalseTarget->Parents.push_back(curBlock);

                curBlock->Instructions.push_back(std::move(branch));
            }

            else if (CFGBlock->TransitionNext.has_value())
            {
                auto jump = std::make_unique<TACJump>();

                jump->TargetBlock = blockMap[CFGBlock->TransitionNext.value()];

                curBlock->Children.push_back(jump->TargetBlock);
                jump->TargetBlock->Parents.push_back(curBlock);

                curBlock->Instructions.push_back(std::move(jump));
            }
        }

        std::reverse(TAC.back()->Blocks.begin(), TAC.back()->Blocks.end());

        NextTemp = 0;
    }

    return TAC;
}

inline void PrintMessage(const Message& msg)
{
    std::string color = "\033[1;30m";

    switch (msg.TranformationType)
    {
        case TACTransformType::DELETED:
            color = "\033[0;91m";
            break;

        case TACTransformType::ADDED:
        case TACTransformType::CLONED:
            color = "\033[0;92m";
            break;

        case TACTransformType::RENAMED:
        case TACTransformType::REPLACED:
            color = "\033[0;93m";
            break;

        case TACTransformType::MOVED:
            color = "\033[0;94m";
            break;
    }

    // └ = U+2514
    // ─ = U+2500
    std::print("\033[0m      └─{}{:<11}\033[0m BY \033[0;96m{:<33}\033[0m : {}\n", color, "[" + TransformationToStr(msg.TranformationType) + "]", "[" + PassToStr(msg.Pass) + "]", msg.Info);
}

void PrintTACInstruction(TACInstruction* inst, bool history)
{
    switch (inst->type)
    {
        case TACType::PHI:
            {
                TACPhi* phi = static_cast<TACPhi*>(inst);

                std::print("    {} = PHI(", TACValToStr(phi->variable));

                for (size_t i = 0; i < phi->args.size(); i++)
                {
                    std::print("{} from B{}", TACValToStr(phi->args[i].Value), phi->args[i].SourceBlock->ID);

                    if (i + 1 != phi->args.size())
                        std::print(", ");
                }

                std::print(")\n");
            }
            break;

        case TACType::ASSIGN:
            {
                TACAssign* assign = static_cast<TACAssign*>(inst);

                std::print("    {} = {}\n", TACValToStr(assign->dest), TACValToStr(assign->source));
            }
            break;

        case TACType::NEG:
            {
                TACNeg* neg = static_cast<TACNeg*>(inst);

                std::print("    {} = neg {}\n", TACValToStr(neg->dest), TACValToStr(neg->source));
            }
            break;

        case TACType::BINARYOP:
            {
                TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst);

                std::print("    {} = {} {} {}\n", TACValToStr(binary->dest), TACValToStr(binary->left), BinaryOpToStr[std::to_underlying(binary->op)], TACValToStr(binary->right));
            }
            break;

        case TACType::CALL:
            {
                TACCall* call = static_cast<TACCall*>(inst);

                if (call->dest.has_value())
                    std::print("    {} = ", TACValToStr(call->dest.value()));
                else
                    std::print("    ");

                std::print("call {}(", call->functionName);

                for (size_t i = 0; i < call->args.size(); i++)
                {
                    std::print("{}", TACValToStr(call->args[i]));

                    if (i + 1 != call->args.size())
                        std::print(", ");
                }

                std::print(")\n");
            }
            break;

        case TACType::BRANCH:
            {
                TACBranch* branch = static_cast<TACBranch*>(inst);

                std::print("    branch ({} {} {}) ? B{} : B{}\n", TACValToStr(branch->cond.Left), BinaryOpToStr[std::to_underlying(branch->cond.Op)], TACValToStr(branch->cond.Right), branch->TrueTarget->ID, branch->FalseTarget->ID);
            }
            break;

        case TACType::SELECT:
            {
                TACSelect* select = static_cast<TACSelect*>(inst);

                std::print("    {} = select ({} {} {}) ? {} : {}\n", TACValToStr(select->dest), TACValToStr(select->cond.Left), BinaryOpToStr[std::to_underlying(select->cond.Op)], TACValToStr(select->cond.Right), TACValToStr(select->TrueVal), TACValToStr(select->FalseVal));
            }
            break;

        case TACType::JUMP:
            {
                TACJump* jump = static_cast<TACJump*>(inst);

                std::print("    jump B{}\n", jump->TargetBlock->ID);
            }
            break;

        case TACType::RETURN:
            {
                TACReturn* ret = static_cast<TACReturn*>(inst);

                if (!ret->ReturnValue.has_value())
                    std::print("    return\n");
                else
                    std::print("    return {}\n", TACValToStr(ret->ReturnValue.value()));
            }
            break;
    }

    std::print("\033[0m");

    if (history)
    {
        for (Message& message : inst->History)
        {
            PrintMessage(message);
        }
    }
}

void PrintTACBlock(TACBlock* block, bool history)
{
    auto blockDead = std::find_if(block->Function->Blocks.begin(), block->Function->Blocks.end(), [&block](auto& b) { return b.get() == block; });

    std::print("\nBlock - {} :\n", block->ID);

    std::print("\033[0m");

    if (history)
    {
        for (Message& message : block->History)
        {
            PrintMessage(message);
        }
    }

    if (history)
    {
        if (block->LastDeadInstruction != nullptr)
        {
            for (auto& prec : block->LastDeadInstruction->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintTACInstruction(prec.get(), history);
            }

            std::print("\033[2;37m");

            PrintTACInstruction(block->LastDeadInstruction.get(), history);

            for (auto& trail : block->LastDeadInstruction->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintTACInstruction(trail.get(), history);
            }
        }
    }

    for (auto& inst : block->Instructions)
    {
        if (history)
        {
            for (auto& prec : inst->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintTACInstruction(prec.get(), history);
            }
        }

        if (blockDead == block->Function->Blocks.end())
            std::print("\033[2;37m");

        PrintTACInstruction(inst.get(), history);

        if (history)
        {
            for (auto& trail : inst->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintTACInstruction(trail.get(), history);
            }
        }
    }
}

void PrintTACFunction(TACFunction* func, bool history)
{
    std::print("# Function - {}(", func->Name);

    for (size_t j = 0; j < func->Parameters.size(); ++j)
    {
        if (j != 0)
            std::print(", ");

        std::print("{}", func->Parameters[j]);
    }

    std::print(") :\n");

    std::print("\033[0m");

    if (history)
    {
        for (Message& message : func->History)
        {
            PrintMessage(message);
        }
    }

    if (history)
    {
        if (func->LastDeadBlock != nullptr)
        {
            for (auto& prec : func->LastDeadBlock->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintTACBlock(prec.get(), history);
            }

            std::print("\033[2;37m");

            PrintTACBlock(func->LastDeadBlock.get(), history);

            for (auto& trail : func->LastDeadBlock->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintTACBlock(trail.get(), history);
            }

            std::print("\n\033[2;37mend {}\033[0m\n\n", func->Name);

            return;
        }
    }

    for (auto& block : func->Blocks)
    {
        if (history)
        {
            for (auto& prec : block->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintTACBlock(prec.get(), history);
            }
        }

        PrintTACBlock(block.get(), history);

        if (history)
        {
            for (auto& trail : block->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintTACBlock(trail.get(), history);
            }
        }
    }

    std::print("\nend {}\n\n", func->Name);
}

void PrintTAC(TAC& TAC, bool history)
{
    if (!history)
        std::print("-----------TAC-----------\n\n");
    else
        std::print("-------TAC-HISTORY-------\n\n");

    for (auto& func : TAC)
    {
        if (history)
        {
            for (auto& prec : func->deadSiblings.preceding)
            {
                std::print("\033[2;37m");

                PrintTACFunction(prec.get(), history);
            }
        }

        PrintTACFunction(func.get(), history);

        if (history)
        {
            for (auto& trail : func->deadSiblings.trailing)
            {
                std::print("\033[2;37m");

                PrintTACFunction(trail.get(), history);
            }
        }
    }

    std::print("\n");
}
