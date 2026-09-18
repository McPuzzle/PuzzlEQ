#pragma once

#include "state/Types.h"
#include "state/BandState.h"
#include <string>
#include <vector>

namespace puzzleq {

struct ChatOp
{
    enum class Kind
    {
        AddOrUpdate,
        ClearAll,
        ApplyPreset,
        Help
    };

    Kind kind = Kind::AddOrUpdate;
    BandState band {};
    int presetIndex = -1;
    std::string label;
};

struct ChatPlan
{
    std::string reply;
    std::vector<ChatOp> ops;
    bool understood = false;
};

ChatPlan parseEqChat (const std::string& userText);
ChatPlan parseEqChatJson (const std::string& jsonText);
std::string eqChatSystemPrompt();
std::string eqChatHelpText();

} // namespace puzzleq
