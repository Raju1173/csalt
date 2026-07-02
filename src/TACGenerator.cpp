#include "TACGenerator.h"
#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <print>

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

TACValue MakeTemp()
{
    return TACValue{"t" + std::to_string(NextTemp++)};
}

TACValue flattenBinaryOpNode(Node *BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>> &Instructions);

std::optional<TACValue> flattenCallNode(Node *CallNode, std::vector<std::unique_ptr<TACInstruction>> &Instructions, bool hasValue = true)
{
    auto call = std::make_unique<TACCall>();

    call->functionName = CallNode->token.lexeme;

    for (auto &child : CallNode->children)
    {
        if (child->children[0]->type == NodeType::BINARY_OP)
            call->args.push_back(flattenBinaryOpNode(child->children[0].get(), Instructions));

        else if (child->children[0]->type == NodeType::CALL)
            call->args.push_back(flattenCallNode(child->children[0].get(), Instructions, true).value());

        else
            call->args.push_back(TACValue{child->children[0]->token.lexeme});
    }

    std::optional<TACValue> returnVal = hasValue ? std::optional<TACValue>{MakeTemp()} : std::nullopt;

    call->dest = returnVal;

    Instructions.push_back(std::move(call));

    return returnVal;
}

TACValue flattenBinaryOpNode(Node *BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>> &Instructions)
{
    auto binaryOp = std::make_unique<TACBinaryOp>();

    if (BinaryOpNode->children[0]->type == NodeType::BINARY_OP)
        binaryOp->left = flattenBinaryOpNode(BinaryOpNode->children[0].get(), Instructions);

    else if (BinaryOpNode->children[0]->type == NodeType::CALL)
        binaryOp->left = flattenCallNode(BinaryOpNode->children[0].get(), Instructions).value();

    else
        binaryOp->left = TACValue{BinaryOpNode->children[0]->token.lexeme};

    binaryOp->op = TokenOpToBinaryOp(BinaryOpNode->token.type);

    if (BinaryOpNode->children[1]->type == NodeType::BINARY_OP)
        binaryOp->right = flattenBinaryOpNode(BinaryOpNode->children[1].get(), Instructions);

    else if (BinaryOpNode->children[1]->type == NodeType::CALL)
        binaryOp->right = flattenCallNode(BinaryOpNode->children[1].get(), Instructions).value();

    else
        binaryOp->right = TACValue{BinaryOpNode->children[1]->token.lexeme};

    TACValue returnVal = MakeTemp();

    binaryOp->dest = returnVal;

    Instructions.push_back(std::move(binaryOp));

    return returnVal;
}

std::vector<std::unique_ptr<TACFunction>> GenerateTAC(std::vector<std::unique_ptr<CFGFunction>> &CFG)
{
    std::vector<std::unique_ptr<TACFunction>> TAC;

    for (auto &CFGFunc : CFG)
    {
        TAC.push_back(std::make_unique<TACFunction>(CFGFunc->FunctionName));

        for (auto &CFGBlock : CFGFunc->Blocks)
        {
            TAC.back()->Blocks.push_back(std::make_unique<TACBlock>(CFGBlock->ID));

            TACBlock *curBlock = TAC.back()->Blocks.back().get();

            for (PhiNode phi : CFGBlock->PhiNodes)
            {
                auto phiInst = std::make_unique<TACPhi>(TACValue{phi.variable + std::to_string(phi.version)});

                phiInst->args = phi.arguments;

                curBlock->Instructions.push_back(std::move(phiInst));
            }

            for (auto &statement : CFGBlock->Statements)
            {
                switch (statement->type)
                {

                    case NodeType::VAR:
                        {
                            auto assign = std::make_unique<TACAssign>();

                            assign->dest = TACValue{statement->token.lexeme};

                            if (statement->children.size() != 0)
                            {
                                if (statement->children[0]->children[0]->type == NodeType::BINARY_OP)
                                    assign->source = flattenBinaryOpNode(statement->children[0]->children[0].get(), curBlock->Instructions);

                                else if (statement->children[0]->children[0]->type == NodeType::CALL)
                                    assign->source = flattenCallNode(statement->children[0]->children[0].get(), curBlock->Instructions).value();

                                else
                                    assign->source = TACValue{statement->children[0]->children[0]->token.lexeme};
                            }

                            else
                            {
                                assign->source = TACValue{"0"};
                            }

                            curBlock->Instructions.push_back(std::move(assign));
                        }
                        break;

                    case NodeType::EXPR:
                        {
                            auto assign = std::make_unique<TACAssign>();

                            assign->dest = TACValue{statement->children[0]->children[0]->token.lexeme};

                            if (statement->children[0]->children[1]->type == NodeType::BINARY_OP)
                                assign->source = flattenBinaryOpNode(statement->children[0]->children[1].get(), curBlock->Instructions);

                            else if (statement->children[0]->children[1]->type == NodeType::CALL)
                                assign->source = flattenCallNode(statement->children[0]->children[1].get(), curBlock->Instructions).value();

                            else
                                assign->source = TACValue{statement->children[0]->children[1]->token.lexeme};

                            curBlock->Instructions.push_back(std::move(assign));
                        }
                        break;

                    case NodeType::CALL:
                        flattenCallNode(statement.get(), curBlock->Instructions, false);
                        break;

                    case NodeType::RETURN:
                        {
                            auto ret = std::make_unique<TACReturn>();

                            if (statement->children.size() != 0)
                            {
                                if (statement->children[0]->children[0]->type == NodeType::BINARY_OP)
                                    ret->ReturnValue = flattenBinaryOpNode(statement->children[0]->children[0].get(), curBlock->Instructions);

                                else if (statement->children[0]->children[0]->type == NodeType::CALL)
                                    ret->ReturnValue = flattenCallNode(statement->children[0]->children[0].get(), curBlock->Instructions).value();

                                else
                                    ret->ReturnValue = TACValue{statement->children[0]->token.lexeme};
                            }

                            else
                                ret->ReturnValue = TACValue{""};

                            curBlock->Instructions.push_back(std::move(ret));
                        }
                        break;
                }
            }

            if (CFGBlock->Condition != nullptr)
            {
                auto branch = std::make_unique<TACBranch>(flattenBinaryOpNode(CFGBlock->Condition->children[0].get(), curBlock->Instructions));

                branch->TrueTarget = CFGBlock->TransitionTrue->ID;
                branch->FalseTarget = CFGBlock->TransitionFalse->ID;

                curBlock->Instructions.push_back(std::move(branch));
            }

            else if (CFGBlock->TransitionNext != nullptr)
            {
                curBlock->Instructions.push_back(std::make_unique<TACJump>(CFGBlock->TransitionNext->ID));
            }
        }

        NextTemp = 0;
    }

    return TAC;
}

void printTAC(std::vector<std::unique_ptr<TACFunction>> &TAC)
{
    for (auto &func : TAC)
    {
        std::print("# Function - {} :\n", func->Name);

        for (auto &block : func->Blocks)
        {
            std::print("\nBlock - {} :\n", block->ID);

            for (auto &inst : block->Instructions)
            {
                if (auto phi = dynamic_cast<TACPhi *>(inst.get()))
                {
                    std::print("    {} = phi(", phi->variable.value);

                    for (size_t i = 0; i < phi->args.size(); i++)
                    {
                        std::print("{} from B{}", phi->args[i].Value, phi->args[i].Pred->ID);

                        if (i + 1 != phi->args.size())
                            std::print(", ");
                    }

                    std::print(")\n");
                }

                else if (auto assign = dynamic_cast<TACAssign *>(inst.get()))
                {
                    std::print("    {} = {}\n", assign->dest.value, assign->source.value);
                }

                else if (auto binary = dynamic_cast<TACBinaryOp *>(inst.get()))
                {
                    std::string op;

                    switch (binary->op)
                    {
                        case BinaryOp::PLUS:
                            op = "+";
                            break;
                        case BinaryOp::MINUS:
                            op = "-";
                            break;
                        case BinaryOp::MUL:
                            op = "*";
                            break;
                        case BinaryOp::DIV:
                            op = "/";
                            break;
                        case BinaryOp::DOUBLE_EQUAL:
                            op = "==";
                            break;
                        case BinaryOp::NOT_EQUAL:
                            op = "!=";
                            break;
                        case BinaryOp::LESS:
                            op = "<";
                            break;
                        case BinaryOp::LESS_EQUAL:
                            op = "<=";
                            break;
                        case BinaryOp::GREATER:
                            op = ">";
                            break;
                        case BinaryOp::GREATER_EQUAL:
                            op = ">=";
                            break;
                    }

                    std::print("    {} = {} {} {}\n", binary->dest.value, binary->left.value, op, binary->right.value);
                }

                else if (auto call = dynamic_cast<TACCall *>(inst.get()))
                {
                    if (call->dest.has_value())
                        std::print("    {} = ", call->dest->value);
                    else
                        std::print("    ");

                    std::print("call {}(", call->functionName);

                    for (size_t i = 0; i < call->args.size(); i++)
                    {
                        std::print("{}", call->args[i].value);

                        if (i + 1 != call->args.size())
                            std::print(", ");
                    }

                    std::print(")\n");
                }

                else if (auto branch = dynamic_cast<TACBranch *>(inst.get()))
                {
                    std::print("    branch {} ? B{} : B{}\n", branch->Condition.value, branch->TrueTarget, branch->FalseTarget);
                }

                else if (auto jump = dynamic_cast<TACJump *>(inst.get()))
                {
                    std::print("    jump B{}\n", jump->TargetBlock);
                }

                else if (auto ret = dynamic_cast<TACReturn *>(inst.get()))
                {
                    if (ret->ReturnValue.value.empty())
                        std::print("    return\n");
                    else
                        std::print("    return {}\n", ret->ReturnValue.value);
                }
            }
        }

        std::print("\n");
    }
}
