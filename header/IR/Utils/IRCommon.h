#pragma once

#include "Globals.h"
#include <iterator>
#include <memory>
#include <print>
#include <string>

enum class IRTransformType
{
    ADDED,

    DELETED,

    MOVED,
    CLONED,

    REPLACED,
    RENAMED,

    COUNT
};

inline std::string IRTransformTypeToStr(IRTransformType transform)
{
    static_assert(std::to_underlying(IRTransformType::COUNT) == 6, "Add/Remove the relevant case from the switch when modifying the IRTransformType enum!!!");

    switch (transform)
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
    Phase Pass;
    IRTransformType TranformationType;
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

    void absorbLeftSiblingCorpse(std::unique_ptr<T>& deadElement)
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

    void absorbRightSiblingCorpse(std::unique_ptr<T>& deadElement)
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
