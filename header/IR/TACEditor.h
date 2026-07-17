#include "TACGenerator.h"
#include <optional>

class TACEditor
{
public:
    static void addInstruction(TACBlock* block, std::unique_ptr<TACInstruction> inst, std::optional<Message> message = std::nullopt);

    static void replaceInstruction(TACBlock* block, TACInstruction* oldInst, std::unique_ptr<TACInstruction> newInst, std::optional<Message> message = std::nullopt);

    static void deleteInstruction(TACBlock* block, TACInstruction* inst, std::optional<Message> message = std::nullopt);

    static void addEdge(TACBlock* From, TACBlock* To);

    static void removeEdge(TACBlock* From, TACBlock* To);

    static void eraseBlock(TACBlock* block, std::optional<Message> message = std::nullopt);
};
