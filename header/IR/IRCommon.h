#pragma once

#include <iterator>
#include <memory>
#include <print>
#include <string>

enum class IRPass
{
    CONSTANT_FOLDING,
    ALGEBRAIC_SIMPLIFICATION,
    BRANCH_SIMPLIFICATION,
    CONTROL_FLOW_SIMPLIFICATION,
    DCE,
    GVN,
    LICM,
    SCO,
    CTFE,

    SSA_RECONSTRUCTION,
};

inline std::string PassToStr(IRPass pass)
{
    switch (pass)
    {
        case IRPass::CONSTANT_FOLDING:
            return "CONSTANT FOLDING";
        case IRPass::ALGEBRAIC_SIMPLIFICATION:
            return "ALGEBRAIC SIMPLIFICATION";
        case IRPass::BRANCH_SIMPLIFICATION:
            return "BRANCH SIMPLIFICATION";
        case IRPass::CONTROL_FLOW_SIMPLIFICATION:
            return "CONTROL FLOW SIMPLIFICATION";
        case IRPass::DCE:
            return "DEAD CODE ELIMINATION";
        case IRPass::GVN:
            return "GLOBAL VALUE NUMBERING";
        case IRPass::LICM:
            return "LOOP INVARIANT CODE MOTION";
        case IRPass::SCO:
            return "SIBLING CALL OPTIMIZATION";
        case IRPass::CTFE:
            return "COMPILE TIME FUNCTION EXECUTION";
        case IRPass::SSA_RECONSTRUCTION:
            return "SSA RECONSTRUCTION";
    }
}

enum class IRTransformType
{
    ADDED,

    DELETED,

    MOVED,
    CLONED,

    REPLACED,
    RENAMED,
};

inline std::string IRTransformationToStr(IRTransformType op)
{
    switch (op)
    {
        case IRTransformType::MOVED:
            return "MOVED";
        case IRTransformType::CLONED:
            return "CLONED";
        case IRTransformType::REPLACED:
            return "REPLACED";
        case IRTransformType::RENAMED:
            return "RENAMED";
        case IRTransformType::ADDED:
            return "ADDED";
        case IRTransformType::DELETED:
            return "DELETED";
    }
}

struct Message
{
    IRPass Pass;
    IRTransformType TranformationType;
    std::string Info;
};

inline void PrintMessage(const Message& msg)
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

    // └ = U+2514
    // ─ = U+2500
    std::print("\033[0m      └─{}{:<11}\033[0m BY \033[0;96m{:<33}\033[0m : {}\n", color, "[" + IRTransformationToStr(msg.TranformationType) + "]", "[" + PassToStr(msg.Pass) + "]", msg.Info);
}

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
