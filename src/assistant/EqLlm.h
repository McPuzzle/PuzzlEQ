#pragma once

#include <string>

namespace puzzleq {

struct LlmSettings
{
    std::string groqKey;
    std::string geminiKey;
    std::string ollamaUrl = "http://127.0.0.1:11434";
    std::string ollamaModel; // empty = first local model
};

struct LlmResult
{
    bool ok = false;
    std::string source;
    std::string text;
    std::string error;
};

LlmSettings loadLlmSettings();
void saveLlmSettings (const LlmSettings&);

// Tries Ollama (no key), then Groq, then Gemini. All free-tier endpoints.
LlmResult requestEqLlm (const std::string& userText,
                        const LlmSettings& settings,
                        const std::string& analysis = {});

} // namespace puzzleq
