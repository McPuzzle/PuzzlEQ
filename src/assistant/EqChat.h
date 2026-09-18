#pragma once

#include "state/Types.h"
#include "state/BandState.h"
#include "assistant/EqAnalyze.h"
#include <array>
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
        Help,
        TweakQ
    };

    Kind kind = Kind::AddOrUpdate;
    BandState band {};
    int presetIndex = -1;
    int targetBand = -1;
    float qMul = 1.0f;
    std::string label;
};

struct ChatContext
{
    int selectedBand = -1;
    std::array<BandState, kMaxBands> bands {};
    SpectrumReport spectrum {};
};

struct ChatPlan
{
    std::string reply;
    std::vector<ChatOp> ops;
    bool understood = false;
    std::string analysisNote;
};

ChatPlan parseEqChat (const std::string& userText, const ChatContext& ctx = {});
ChatPlan parseEqChatJson (const std::string& jsonText);
std::string eqChatSystemPrompt();
std::string eqChatHelpText();

} // namespace puzzleq
