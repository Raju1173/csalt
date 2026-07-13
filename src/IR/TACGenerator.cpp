#include "TACGenerator.h"
#include "CFGBuilder.h"
#include "lexer.h"
#include "parser.h"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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

TACValue flattenBinaryOpNode(Node& BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions);

std::optional<TACValue> flattenCallNode(Node& CallNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions, bool hasValue = true)
{
    auto call = std::make_unique<TACCall>();

    call->functionName = CallNode.token.lexeme;

    for (Node& child : CallNode.children)
    {
        if (child.children[0].type == NodeType::BINARY_OP)
            call->args.push_back(flattenBinaryOpNode(child.children[0], Instructions));

        else if (child.children[0].type == NodeType::CALL)
            call->args.push_back(flattenCallNode(child.children[0], Instructions, true).value());

        else
            call->args.push_back(TACValue{child.children[0].token.lexeme});
    }

    std::optional<TACValue> returnVal = hasValue ? std::optional<TACValue>{MakeTemp()} : std::nullopt;

    call->dest = returnVal;

    Instructions.push_back(std::move(call));

    return returnVal;
}

TACValue flattenBinaryOpNode(Node& BinaryOpNode, std::vector<std::unique_ptr<TACInstruction>>& Instructions)
{
    auto binaryOp = std::make_unique<TACBinaryOp>();

    if (BinaryOpNode.children[0].type == NodeType::BINARY_OP)
        binaryOp->left = flattenBinaryOpNode(BinaryOpNode.children[0], Instructions);

    else if (BinaryOpNode.children[0].type == NodeType::CALL)
        binaryOp->left = flattenCallNode(BinaryOpNode.children[0], Instructions).value();

    else
        binaryOp->left = TACValue{BinaryOpNode.children[0].token.lexeme};

    binaryOp->op = TokenOpToBinaryOp(BinaryOpNode.token.type);

    if (BinaryOpNode.children[1].type == NodeType::BINARY_OP)
        binaryOp->right = flattenBinaryOpNode(BinaryOpNode.children[1], Instructions);

    else if (BinaryOpNode.children[1].type == NodeType::CALL)
        binaryOp->right = flattenCallNode(BinaryOpNode.children[1], Instructions).value();

    else
        binaryOp->right = TACValue{BinaryOpNode.children[1].token.lexeme};

    TACValue returnVal = MakeTemp();

    binaryOp->dest = returnVal;

    Instructions.push_back(std::move(binaryOp));

    return returnVal;
}

TAC GenerateTAC(CFG& CFG)
{
    TAC TAC;

    for (CFGFunction& CFGFunc : CFG)
    {
        TAC.push_back(TACFunction{CFGFunc.FunctionName, CFGFunc.Parameters});

        for (auto& CFGBlock : CFGFunc.Blocks)
        {
            TAC.back().Blocks.push_back(std::make_unique<TACBlock>(CFGBlock->ID));

            TACBlock* curBlock = TAC.back().Blocks.back().get();

            for (PhiNode phi : CFGBlock->PhiNodes)
            {
                auto phiInst = std::make_unique<TACPhi>(TACValue{phi.variable + std::to_string(phi.version)});

                phiInst->args = phi.arguments;

                curBlock->Instructions.push_back(std::move(phiInst));
            }

            for (Node& statement : CFGBlock->Statements)
            {
                switch (statement.type)
                {
                    case NodeType::VAR:
                        {
                            auto assign = std::make_unique<TACAssign>();

                            assign->dest = TACValue{statement.token.lexeme};

                            if (statement.children.size() != 0)
                            {
                                if (statement.children[0].children[0].type == NodeType::BINARY_OP)
                                    assign->source = flattenBinaryOpNode(statement.children[0].children[0], curBlock->Instructions);

                                else if (statement.children[0].children[0].type == NodeType::CALL)
                                    assign->source = flattenCallNode(statement.children[0].children[0], curBlock->Instructions).value();

                                else
                                    assign->source = TACValue{statement.children[0].children[0].token.lexeme};
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

                            assign->dest = TACValue{statement.children[0].children[0].token.lexeme};

                            if (statement.children[0].children[1].type == NodeType::BINARY_OP)
                                assign->source = flattenBinaryOpNode(statement.children[0].children[1], curBlock->Instructions);

                            else if (statement.children[0].children[1].type == NodeType::CALL)
                                assign->source = flattenCallNode(statement.children[0].children[1], curBlock->Instructions).value();

                            else
                                assign->source = TACValue{statement.children[0].children[1].token.lexeme};

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

                                else if (statement.children[0].children[0].type == NodeType::CALL)
                                    ret->ReturnValue = flattenCallNode(statement.children[0].children[0], curBlock->Instructions).value();

                                else
                                    ret->ReturnValue = TACValue{statement.children[0].children[0].token.lexeme};
                            }

                            else
                                ret->ReturnValue = TACValue{""};

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

                branch->TrueTarget = CFGBlock->TransitionTrue.value()->ID;
                branch->FalseTarget = CFGBlock->TransitionFalse.value()->ID;

                curBlock->Instructions.push_back(std::move(branch));
            }

            else if (CFGBlock->TransitionNext.has_value())
            {
                curBlock->Instructions.push_back(std::make_unique<TACJump>(CFGBlock->TransitionNext.value()->ID));
            }
        }

        NextTemp = 0;
    }

    for (TACFunction& TACFunc : TAC)
    {
        for (auto& block : TACFunc.Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::BRANCH)
                {
                    TACBranch* branch = static_cast<TACBranch*>(inst.get());

                    TACBlock* trueTarget = (*(std::find_if(TACFunc.Blocks.begin(), TACFunc.Blocks.end(), [&branch](const auto& b) { return b->ID == branch->TrueTarget; }))).get();
                    TACBlock* falseTarget = (*(std::find_if(TACFunc.Blocks.begin(), TACFunc.Blocks.end(), [&branch](const auto& b) { return b->ID == branch->FalseTarget; }))).get();

                    block->Children.push_back(trueTarget);
                    block->Children.push_back(falseTarget);

                    trueTarget->Parents.push_back(block.get());
                    falseTarget->Parents.push_back(block.get());
                }

                else if (inst->type == TACType::JUMP)
                {
                    TACJump* jump = static_cast<TACJump*>(inst.get());

                    TACBlock* target = (*(std::find_if(TACFunc.Blocks.begin(), TACFunc.Blocks.end(), [&jump](const auto& b) { return b->ID == jump->TargetBlock; }))).get();

                    block->Children.push_back(target);

                    target->Parents.push_back(block.get());
                }
            }
        }
    }

    return TAC;
}

void ResolvePhiNodes(TAC& TAC)
{
    for (TACFunction& TACFunc : TAC)
    {
        for (auto& block : TACFunc.Blocks)
        {
            for (auto& inst : block->Instructions)
            {
                if (inst->type == TACType::PHI)
                {
                    TACPhi* phi = static_cast<TACPhi*>(inst.get());

                    for (PhiArgument& arg : phi->args)
                    {
                        for (auto& sourceBlock : TACFunc.Blocks)
                        {
                            if (sourceBlock->ID == arg.SourceID)
                            {
                                auto assign = std::make_unique<TACAssign>();

                                assign->dest = phi->variable;

                                assign->source = TACValue{arg.Value};

                                if (sourceBlock->Instructions.size() >= 1)
                                    sourceBlock->Instructions.insert(sourceBlock->Instructions.end() - 1, std::move(assign));

                                else
                                    sourceBlock->Instructions.push_back(std::move(assign));

                                break;
                            }
                        }
                    }
                }
            }

            std::erase_if(block->Instructions, [](const auto& inst) { return inst->type == TACType::PHI; });
        }
    }
}

TACDominatorInfo& TAC::computeDominators(TACFunction& TACFunc)
{
    TACDominatorInfo& DomInfo = getDominatorInfo(TACFunc.Name);

    if (!DomInfo.isValid)
    {
        DomInfo.isValid = true;

        DomInfo.Dominators.clear();

        if (TACFunc.Blocks.empty())
            return DomInfo;

        auto& Blocks = TACFunc.Blocks;

        TACBlock* entryBlock = Blocks[0].get();

        std::unordered_set<TACBlock*> universalSet;

        for (auto& block : Blocks)
        {
            universalSet.insert(block.get());
        }

        DomInfo.Dominators[entryBlock] = {entryBlock};

        for (size_t j = 1; j < Blocks.size(); ++j)
        {
            DomInfo.Dominators[Blocks[j].get()] = universalSet;
        }

        bool changed = true;

        while (changed)
        {
            changed = false;

            for (size_t j = 0; j < Blocks.size(); j++)
            {
                TACBlock* curBlock = Blocks[j].get();

                if (&curBlock == &entryBlock)
                    continue;

                std::unordered_set<TACBlock*> NewDominators;

                if (!curBlock->Parents.empty())
                {
                    NewDominators = DomInfo.Dominators[curBlock->Parents[0]];

                    for (size_t k = 1; k < curBlock->Parents.size(); k++)
                    {
                        if (NewDominators.empty())
                            break;

                        std::unordered_set<TACBlock*> currentIntersection;

                        for (TACBlock* dom : NewDominators)
                        {
                            if (DomInfo.Dominators[curBlock->Parents[k]].contains(dom))
                            {
                                currentIntersection.insert(dom);
                            }
                        }

                        NewDominators = std::move(currentIntersection);
                    }
                }

                NewDominators.insert(curBlock);

                if (NewDominators != DomInfo.Dominators[curBlock])
                {
                    DomInfo.Dominators[curBlock] = std::move(NewDominators);
                    changed = true;
                }
            }
        }
    }

    return DomInfo;
}

void TAC::computeDominators()
{
    for (TACFunction& Func : Functions)
        computeDominators(Func);
}

TACDominatorTreeInfo& TAC::computeDominatorTree(TACFunction& TACFunc)
{
    TACDominatorInfo& DomInfo = getDominatorInfo(TACFunc.Name);
    TACDominatorTreeInfo& DomTreeInfo = getDominatorTreeInfo(TACFunc.Name);

    TACBlock* nearestDominator = nullptr;
    size_t maxSize = 0;

    if (!DomTreeInfo.isValid)
    {
        DomTreeInfo.isValid = true;

        DomTreeInfo.DominatorTree.clear();

        for (auto& block : TACFunc.Blocks)
        {
            for (TACBlock* dom : DomInfo.Dominators[block.get()])
            {
                if (dom == block.get())
                    continue;

                size_t size = DomInfo.Dominators[dom].size();

                if (size > maxSize)
                {
                    maxSize = size;

                    nearestDominator = dom;
                }
            }

            if (nearestDominator != nullptr)
            {
                DomTreeInfo.DominatorTree[nearestDominator].push_back(block.get());

                nearestDominator = nullptr;
                maxSize = 0;
            }
        }
    }

    return DomTreeInfo;
}

void TAC::computeDominatorTree()
{
    for (TACFunction& Func : Functions)
        computeDominatorTree(Func);
}

TACVarUsesInfo& TAC::computeVarUses(TACFunction& TACFunc)
{
    TACVarUsesInfo& VarUsesInfo = getVarUsesInfo(TACFunc.Name);

    if (!VarUsesInfo.isValid)
    {
        VarUsesInfo.isValid = true;

        VarUsesInfo.VarUses.clear();

        for (const auto& Block : TACFunc.Blocks)
        {
            for (const auto& inst : Block->Instructions)
            {
                switch (inst->type)
                {
                    case TACType::PHI:
                        {
                            TACPhi* phi = static_cast<TACPhi*>(inst.get());

                            for (PhiArgument& arg : phi->args)
                                VarUsesInfo.VarUses[arg.Value]++;
                        }
                        break;

                    case TACType::ASSIGN:
                        {
                            TACAssign* assign = static_cast<TACAssign*>(inst.get());

                            VarUsesInfo.VarUses[assign->source.value]++;
                        }
                        break;

                    case TACType::BINARYOP:
                        {
                            TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                            VarUsesInfo.VarUses[binary->left.value]++;
                            VarUsesInfo.VarUses[binary->right.value]++;
                        }
                        break;

                    case TACType::CALL:
                        {
                            TACCall* call = static_cast<TACCall*>(inst.get());

                            for (const auto& arg : call->args)
                            {
                                VarUsesInfo.VarUses[arg.value]++;
                            }
                        }
                        break;

                    case TACType::BRANCH:
                        {
                            TACBranch* branch = static_cast<TACBranch*>(inst.get());

                            VarUsesInfo.VarUses[branch->cond.Left.value]++;
                            VarUsesInfo.VarUses[branch->cond.Right.value]++;
                        }
                        break;

                    case TACType::RETURN:
                        {
                            TACReturn* ret = static_cast<TACReturn*>(inst.get());

                            VarUsesInfo.VarUses[ret->ReturnValue.value]++;
                        }
                        break;
                }
            }
        }
    }

    return VarUsesInfo;
}

void TAC::computeVarUses()
{
    for (TACFunction& Func : Functions)
        computeVarUses(Func);
}

void PrintTAC(TAC& TAC)
{
    std::print("------TAC-------\n\n");

    for (TACFunction& func : TAC)
    {
        std::print("# Function(");

        for (size_t j = 0; j < func.Parameters.size(); ++j)
        {
            if (j != 0)
                std::print(", ");

            std::print("{}", func.Parameters[j]);
        }

        std::print(") - {} :\n", func.Name);

        for (auto& block : func.Blocks)
        {
            std::print("\nBlock - {} :\n", block->ID);

            for (auto& inst : block->Instructions)
            {
                switch (inst->type)
                {
                    case TACType::PHI:
                        {
                            TACPhi* phi = static_cast<TACPhi*>(inst.get());

                            std::print("    {} = phi(", phi->variable.value);

                            for (size_t i = 0; i < phi->args.size(); i++)
                            {
                                std::print("{} from B{}", phi->args[i].Value, phi->args[i].SourceID);

                                if (i + 1 != phi->args.size())
                                    std::print(", ");
                            }

                            std::print(")\n");
                        }
                        break;

                    case TACType::ASSIGN:
                        {
                            TACAssign* assign = static_cast<TACAssign*>(inst.get());

                            std::print("    {} = {}\n", assign->dest.value, (assign->source.neg ? "-" : "") + assign->source.value);
                        }
                        break;

                    case TACType::BINARYOP:
                        {
                            TACBinaryOp* binary = static_cast<TACBinaryOp*>(inst.get());

                            std::print("    {} = {} {} {}\n", binary->dest.value, binary->left.value, BinaryOpToStr[std::to_underlying(binary->op)], binary->right.value);
                        }
                        break;

                    case TACType::CALL:
                        {
                            TACCall* call = static_cast<TACCall*>(inst.get());

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
                        break;

                    case TACType::BRANCH:
                        {
                            TACBranch* branch = static_cast<TACBranch*>(inst.get());

                            std::print("    branch ({} {} {}) ? B{} : B{}\n", branch->cond.Left.value, BinaryOpToStr[std::to_underlying(branch->cond.Op)], branch->cond.Right.value, branch->TrueTarget, branch->FalseTarget);
                        }
                        break;

                    case TACType::JUMP:
                        {
                            TACJump* jump = static_cast<TACJump*>(inst.get());

                            std::print("    jump B{}\n", jump->TargetBlock);
                        }
                        break;

                    case TACType::RETURN:
                        {
                            TACReturn* ret = static_cast<TACReturn*>(inst.get());

                            if (ret->ReturnValue.value.empty())
                                std::print("    return\n");
                            else
                                std::print("    return {}\n", ret->ReturnValue.value);
                        }
                        break;
                }
            }
        }

        std::print("\n");
    }

    std::print("\n");
}

bool isConstant(std::string str)
{
    int value;

    auto [ptr, err] = std::from_chars(str.data(), str.data() + str.size(), value);

    return err == std::errc{} && ptr == str.data() + str.size();
}
